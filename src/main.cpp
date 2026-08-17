#include <chrono>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "stipple/animation.hpp"
#include "stipple/config.hpp"
#include "stipple/image.hpp"
#include "stipple/lloyd.hpp"
#include "stipple/renderer.hpp"

namespace {

struct CliOptions {
    std::string input_path;
    std::string output_path = "stipple.png";
    std::string animation_path;
    std::string compare_dir;
    stipple::LloydConfig config;

};

struct RunSummary {
    stipple::Backend backend = stipple::Backend::Serial;
    bool passed = false;
    std::string output_path;
    std::string animation_path;
    std::string error;
    int completed_iterations = 0;
    float final_average_movement = 0.0f;
    long long elapsed_ms = 0;
};

void print_usage(const char* executable) {
    std::cout
        << "Usage:\n"
        << "  " << executable << " --input image.png --output out.png [options]\n\n"
        << "Options:\n"
        << "  --input PATH              Input image path\n"
        << "  --output PATH             Final stipple image path [default: stipple.png]\n"
        << "  --points N                Number of stipple points [default: 1000]\n"
        << "  --iterations N            Lloyd iterations [default: 20]\n"
        << "  --epsilon VALUE           Early-stop average movement [default: 0.001]\n"
        << "  --seed N                  Random seed [default: 42]\n"
        << "  --mode MODE               serial | serial-simd | openmp | openmp-simd | cuda | all\n"
        << "  --animation PATH          Write MP4 animation; with --mode all, mode name is appended\n"
        << "  --compare-dir PATH        Text report directory [default: derived from output or test/compare]\n"
        << "  --help                    Show this help\n";
}

std::string require_value(int& index, int argc, char** argv, const std::string& option) {
    if (index + 1 >= argc) {
        throw std::invalid_argument("Missing value for " + option);
    }
    ++index;
    return argv[index];
}

std::string timestamp_string() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    tm = *std::localtime(&time);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d-%H%M%S");
    return oss.str();
}

std::string append_suffix_to_path(const std::string& path, const std::string& suffix) {
    const std::filesystem::path original(path);
    const std::filesystem::path parent = original.parent_path();
    const std::string stem = original.stem().string();
    const std::string extension = original.extension().string();
    const std::filesystem::path filename = stem + suffix + extension;
    return parent.empty() ? filename.string() : (parent / filename).string();
}

std::string derive_compare_dir(const CliOptions& options) {
    if (!options.compare_dir.empty()) {
        return options.compare_dir;
    }

    const std::filesystem::path output(options.output_path);
    const std::filesystem::path parent = output.parent_path();
    if (parent.filename() == "out" && parent.parent_path().filename() == "test") {
        return (parent.parent_path() / "compare").string();
    }

    return "test/compare";
}

std::string make_report_path(const CliOptions& options) {
    const std::filesystem::path input(options.input_path);
    const std::string input_stem = input.stem().empty() ? "image" : input.stem().string();
    const std::string mode = stipple::to_string(options.config.backend);
    const std::filesystem::path compare_dir = derive_compare_dir(options);
    return (compare_dir / ("compare-" + input_stem + "-" + mode + "-" + timestamp_string() + ".txt")).string();
}

std::vector<stipple::Backend> all_backends() {
    return {
        stipple::Backend::Serial,
        stipple::Backend::SerialSIMD,
        stipple::Backend::OpenMP,
        stipple::Backend::OpenMPSIMD,
        stipple::Backend::CUDA,
    };
}

CliOptions parse_cli(int argc, char** argv) {
    CliOptions options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (arg == "--input") {
            options.input_path = require_value(i, argc, argv, arg);
        } else if (arg == "--output") {
            options.output_path = require_value(i, argc, argv, arg);
        } else if (arg == "--points") {
            options.config.point_count = static_cast<std::size_t>(std::stoull(require_value(i, argc, argv, arg)));
        } else if (arg == "--iterations") {
            options.config.iterations = std::stoi(require_value(i, argc, argv, arg));
        } else if (arg == "--epsilon") {
            options.config.epsilon = std::stof(require_value(i, argc, argv, arg));
        } else if (arg == "--seed") {
            options.config.seed = static_cast<unsigned int>(std::stoul(require_value(i, argc, argv, arg)));
        } else if (arg == "--mode") {
            options.config.backend = stipple::parse_backend(require_value(i, argc, argv, arg));
        } else if (arg == "--animation") {
            options.animation_path = require_value(i, argc, argv, arg);
            options.config.animate = true;
        } else if (arg == "--compare-dir") {
            options.compare_dir = require_value(i, argc, argv, arg);
        } else {
            throw std::invalid_argument("Unknown option: " + arg);
        }
    }

    if (options.input_path.empty()) {
        throw std::invalid_argument("--input is required");
    }
    if (options.config.point_count == 0) {
        throw std::invalid_argument("--points must be greater than zero");
    }
    if (options.config.iterations <= 0) {
        throw std::invalid_argument("--iterations must be greater than zero");
    }

    return options;
}

RunSummary run_one_backend(
    const stipple::Image& image,
    const CliOptions& options,
    stipple::Backend backend,
    const std::string& output_path,
    const std::string& animation_path) {
    RunSummary summary;
    summary.backend = backend;
    summary.output_path = output_path;
    summary.animation_path = animation_path;

    try {
        stipple::LloydConfig config = options.config;
        config.backend = backend;
        config.animate = !animation_path.empty();

        std::optional<stipple::StippleAnimationWriter> animation;
        stipple::IterationCallback callback;
        if (config.animate) {
            animation.emplace(animation_path, image.width, image.height, 24.0);
            callback = [&](int, const stipple::Points& points) {
                animation->write_frame(points);
            };
        }

        const auto start = std::chrono::steady_clock::now();
        const stipple::LloydResult result = stipple::run_lloyd(image, config, callback);
        const auto end = std::chrono::steady_clock::now();

        if (animation) {
            animation->close();
        }

        stipple::render_stipple_image(result.points, image.width, image.height, output_path);

        summary.passed = true;
        summary.completed_iterations = result.completed_iterations;
        summary.final_average_movement = result.final_average_movement;
        summary.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    } catch (const std::exception& error) {
        summary.passed = false;
        summary.error = error.what();
    }

    return summary;
}

void write_report(const CliOptions& options, const std::string& report_path, const std::vector<RunSummary>& summaries) {
    const std::filesystem::path report(report_path);
    if (!report.parent_path().empty()) {
        std::filesystem::create_directories(report.parent_path());
    }

    std::ofstream file(report_path);
    if (!file) {
        throw std::runtime_error("Failed to write comparison report: " + report_path);
    }

    file << "Stipple run report\n"
         << "==================\n"
         << "Input: " << options.input_path << '\n'
         << "Requested mode: " << stipple::to_string(options.config.backend) << '\n'
         << "Points: " << options.config.point_count << '\n'
         << "Iterations: " << options.config.iterations << '\n'
         << "Epsilon: " << options.config.epsilon << '\n'
         << "Seed: " << options.config.seed << '\n'
         << "Report: " << report_path << "\n\n";

    for (const RunSummary& summary : summaries) {
        file << "----------------------------------------\n"
             << "Mode: " << stipple::to_string(summary.backend) << '\n'
             << "Status: " << (summary.passed ? "passed" : "failed") << '\n'
             << "Output: " << summary.output_path << '\n';

        if (!summary.animation_path.empty()) {
            file << "Animation: " << summary.animation_path << '\n';
        }

        if (summary.passed) {
            file << "Iterations completed: " << summary.completed_iterations << '\n'
                 << "Final average movement: " << summary.final_average_movement << '\n'
                 << "Elapsed: " << summary.elapsed_ms << " ms\n";
        } else {
            file << "Error: " << summary.error << '\n';
        }

        file << '\n';
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions options = parse_cli(argc, argv);
        const stipple::Image image = stipple::load_density_image(options.input_path);


        std::vector<RunSummary> summaries;
        if (options.config.backend == stipple::Backend::All) {
            for (const stipple::Backend backend : all_backends()) {
                const std::string mode_suffix = "-" + stipple::to_string(backend);
                const std::string output_path = append_suffix_to_path(options.output_path, mode_suffix);
                const std::string animation_path = options.config.animate
                    ? append_suffix_to_path(options.animation_path, mode_suffix)
                    : std::string{};

                std::cout << "Running mode: " << stipple::to_string(backend) << '\n';
                RunSummary summary = run_one_backend(image, options, backend, output_path, animation_path);
                summaries.push_back(summary);

                if (summary.passed) {
                    std::cout << "  passed: " << output_path << " (" << summary.elapsed_ms << " ms)\n";
                } else {
                    std::cout << "  failed: " << summary.error << '\n';
                }
            }
        } else {
            summaries.push_back(run_one_backend(image, options, options.config.backend, options.output_path, options.animation_path));

            const RunSummary& summary = summaries.front();
            if (!summary.passed) {
                throw std::runtime_error(summary.error);
            }

            std::cout << "Backend: " << stipple::to_string(summary.backend) << '\n'
                      << "Points: " << options.config.point_count << '\n'
                      << "Iterations completed: " << summary.completed_iterations << '\n'
                      << "Final average movement: " << summary.final_average_movement << '\n'
                      << "Elapsed: " << summary.elapsed_ms << " ms\n"
                      << "Wrote final image: " << summary.output_path << '\n';

            if (!summary.animation_path.empty()) {
                std::cout << "Wrote animation: " << summary.animation_path << '\n';
            }
        }

        const std::string report_path = make_report_path(options);
        write_report(options, report_path, summaries);
        std::cout << "Wrote comparison report: " << report_path << '\n';

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n\n";
        print_usage(argv[0]);
        return 1;
    }
}
