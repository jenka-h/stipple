#include <chrono>
#include <exception>
#include <optional>
#include <string>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include "stipple/animation.hpp"
#include "stipple/config.hpp"
#include "stipple/image.hpp"
#include "stipple/lloyd.hpp"
#include "stipple/renderer.hpp"

namespace {

stipple::Backend backend_from_text(const QString& text) {
    return stipple::parse_backend(text.toStdString());
}

void run_stipple(
    QWidget* parent,
    QLineEdit* input_edit,
    QLineEdit* output_edit,
    QComboBox* backend_combo,
    QSpinBox* points_spin,
    QSpinBox* iterations_spin,
    QDoubleSpinBox* epsilon_spin,
    QSpinBox* seed_spin,
    QCheckBox* animation_check,
    QLineEdit* animation_edit,
    QLabel* status_label,
    QPushButton* run_button) {
    run_button->setEnabled(false);
    status_label->setText("Running...");
    QApplication::processEvents();

    try {
        stipple::LloydConfig config;
        config.point_count = static_cast<std::size_t>(points_spin->value());
        config.iterations = iterations_spin->value();
        config.epsilon = static_cast<float>(epsilon_spin->value());
        config.seed = static_cast<unsigned int>(seed_spin->value());
        config.backend = backend_from_text(backend_combo->currentText());
        config.animate = animation_check->isChecked();

        const std::string input_path = input_edit->text().toStdString();
        const std::string output_path = output_edit->text().toStdString();
        const std::string animation_path = animation_edit->text().toStdString();

        if (input_path.empty()) {
            throw std::runtime_error("Input path is required.");
        }
        if (output_path.empty()) {
            throw std::runtime_error("Output path is required.");
        }
        if (config.animate && animation_path.empty()) {
            throw std::runtime_error("Animation path is required when animation is enabled.");
        }

        const auto start = std::chrono::steady_clock::now();
        const stipple::Image image = stipple::load_density_image(input_path);

        std::optional<stipple::StippleAnimationWriter> animation;
        stipple::IterationCallback callback;
        if (config.animate) {
            animation.emplace(animation_path, image.width, image.height, 24.0);
            callback = [&](int, const stipple::Points& points) {
                animation->write_frame(points);
            };
        }

        const stipple::LloydResult result = stipple::run_lloyd(image, config, callback);
        if (animation) {
            animation->close();
        }

        stipple::render_stipple_image(result.points, image.width, image.height, output_path);

        const auto end = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        const QString message = QString(
            "Done.\n\n"
            "Output: %1\n"
            "Backend: %2\n"
            "Iterations completed: %3\n"
            "Final average movement: %4\n"
            "Elapsed: %5 ms")
            .arg(QString::fromStdString(output_path))
            .arg(QString::fromStdString(stipple::to_string(config.backend)))
            .arg(result.completed_iterations)
            .arg(result.final_average_movement)
            .arg(elapsed_ms);

        status_label->setText("Done: " + QString::fromStdString(output_path));
        QMessageBox::information(parent, "Stipple Complete", message);
    } catch (const std::exception& error) {
        status_label->setText("Failed.");
        QMessageBox::critical(parent, "Stipple Failed", error.what());
    }

    run_button->setEnabled(true);
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Stipple Qt6 GUI");
    window.resize(620, 360);

    auto* root_layout = new QVBoxLayout(&window);

    auto* form_box = new QGroupBox("Stipple Settings");
    auto* form_layout = new QFormLayout(form_box);

    auto* input_edit = new QLineEdit("test/in/1.jpg");
    auto* input_browse = new QPushButton("Browse...");
    auto* input_row = new QWidget();
    auto* input_row_layout = new QHBoxLayout(input_row);
    input_row_layout->setContentsMargins(0, 0, 0, 0);
    input_row_layout->addWidget(input_edit);
    input_row_layout->addWidget(input_browse);
    form_layout->addRow("Input image", input_row);

    auto* output_edit = new QLineEdit("test/out/gui-output.png");
    auto* output_browse = new QPushButton("Browse...");
    auto* output_row = new QWidget();
    auto* output_row_layout = new QHBoxLayout(output_row);
    output_row_layout->setContentsMargins(0, 0, 0, 0);
    output_row_layout->addWidget(output_edit);
    output_row_layout->addWidget(output_browse);
    form_layout->addRow("Output image", output_row);

    auto* backend_combo = new QComboBox();
    backend_combo->addItems({"serial", "serial-simd", "openmp", "openmp-simd", "cuda"});
    backend_combo->setCurrentText("openmp-simd");
    form_layout->addRow("Backend", backend_combo);

    auto* points_spin = new QSpinBox();
    points_spin->setRange(1, 10000000);
    points_spin->setValue(1000);
    form_layout->addRow("Points", points_spin);

    auto* iterations_spin = new QSpinBox();
    iterations_spin->setRange(1, 100000);
    iterations_spin->setValue(20);
    form_layout->addRow("Iterations", iterations_spin);

    auto* epsilon_spin = new QDoubleSpinBox();
    epsilon_spin->setRange(0.0, 1000000.0);
    epsilon_spin->setDecimals(6);
    epsilon_spin->setSingleStep(0.001);
    epsilon_spin->setValue(0.001);
    form_layout->addRow("Epsilon", epsilon_spin);

    auto* seed_spin = new QSpinBox();
    seed_spin->setRange(0, 2147483647);
    seed_spin->setValue(42);
    form_layout->addRow("Seed", seed_spin);

    auto* animation_check = new QCheckBox("Write MP4 animation");
    form_layout->addRow("Animation", animation_check);

    auto* animation_edit = new QLineEdit("test/out/gui-output.mp4");
    auto* animation_browse = new QPushButton("Browse...");
    auto* animation_row = new QWidget();
    auto* animation_row_layout = new QHBoxLayout(animation_row);
    animation_row_layout->setContentsMargins(0, 0, 0, 0);
    animation_row_layout->addWidget(animation_edit);
    animation_row_layout->addWidget(animation_browse);
    animation_row->setEnabled(false);
    form_layout->addRow("Animation file", animation_row);

    root_layout->addWidget(form_box);

    auto* button_row = new QHBoxLayout();
    auto* run_button = new QPushButton("Run Stipple");
    auto* status_label = new QLabel("Ready.");
    button_row->addWidget(run_button);
    button_row->addWidget(status_label, 1);
    root_layout->addLayout(button_row);

    QObject::connect(input_browse, &QPushButton::clicked, [&]() {
        const QString path = QFileDialog::getOpenFileName(&window, "Select input image", "test/in");
        if (!path.isEmpty()) {
            input_edit->setText(path);
        }
    });

    QObject::connect(output_browse, &QPushButton::clicked, [&]() {
        const QString path = QFileDialog::getSaveFileName(&window, "Select output image", "test/out/gui-output.png", "Images (*.png *.jpg *.jpeg)");
        if (!path.isEmpty()) {
            output_edit->setText(path);
        }
    });

    QObject::connect(animation_check, &QCheckBox::toggled, animation_row, &QWidget::setEnabled);

    QObject::connect(animation_browse, &QPushButton::clicked, [&]() {
        const QString path = QFileDialog::getSaveFileName(&window, "Select animation output", "test/out/gui-output.mp4", "Videos (*.mp4 *.avi)");
        if (!path.isEmpty()) {
            animation_edit->setText(path);
        }
    });

    QObject::connect(run_button, &QPushButton::clicked, [&]() {
        run_stipple(
            &window,
            input_edit,
            output_edit,
            backend_combo,
            points_spin,
            iterations_spin,
            epsilon_spin,
            seed_spin,
            animation_check,
            animation_edit,
            status_label,
            run_button);
    });

    window.show();
    return app.exec();
}
