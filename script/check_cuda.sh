#!/usr/bin/env bash
set -u

usage() {
    cat <<'USAGE'
Usage:
  ./script/check_cuda.sh [options]

Options:
  --help        Show this help message
  --export      Print export commands for CUDA_HOME, PATH, and LD_LIBRARY_PATH
  --cmake       Print a CMake configure command with CUDA enabled

Examples:
  ./script/check_cuda.sh
  ./script/check_cuda.sh --export
  eval "$(./script/check_cuda.sh --export)"
  ./script/check_cuda.sh --cmake
USAGE
}

want_export=0
want_cmake=0

for arg in "$@"; do
    case "$arg" in
        --help|-h)
            usage
            exit 0
            ;;
        --export)
            want_export=1
            ;;
        --cmake)
            want_cmake=1
            ;;
        *)
            echo "Unknown option: $arg" >&2
            usage >&2
            exit 2
            ;;
    esac
done

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

find_nvcc() {
    if command_exists nvcc; then
        command -v nvcc
        return 0
    fi

    for candidate in \
        /usr/local/cuda/bin/nvcc \
        /opt/cuda/bin/nvcc \
        /usr/bin/nvcc; do
        if [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

infer_cuda_home() {
    nvcc_path="$1"
    cuda_bin_dir="$(dirname "$nvcc_path")"
    cuda_home="$(dirname "$cuda_bin_dir")"
    printf '%s\n' "$cuda_home"
}

find_cuda_home_without_nvcc() {
    for candidate in \
        /usr/local/cuda \
        /opt/cuda; do
        if [ -d "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    latest=""
    for candidate in /usr/local/cuda-*; do
        if [ -d "$candidate" ]; then
            latest="$candidate"
        fi
    done

    if [ -n "$latest" ]; then
        printf '%s\n' "$latest"
        return 0
    fi

    return 1
}

print_status() {
    label="$1"
    value="$2"
    printf '%-22s %s\n' "$label" "$value"
}

nvcc_path=""
cuda_home=""
cuda_available=0
cuda_runtime_available=0

if nvcc_path="$(find_nvcc)"; then
    cuda_available=1
    cuda_home="$(infer_cuda_home "$nvcc_path")"
elif cuda_home="$(find_cuda_home_without_nvcc)"; then
    cuda_available=0
else
    cuda_available=0
fi

if [ "$want_export" -eq 1 ]; then
    if [ -z "$cuda_home" ]; then
        echo "# CUDA toolkit was not found. No exports emitted."
        exit 1
    fi

    printf 'export CUDA_HOME=%q\n' "$cuda_home"
    printf 'export CUDAToolkit_ROOT=%q\n' "$cuda_home"
    printf 'export PATH=%q:"$PATH"\n' "$cuda_home/bin"

    if [ -d "$cuda_home/lib64" ]; then
        printf 'export LD_LIBRARY_PATH=%q:"${LD_LIBRARY_PATH:-}"\n' "$cuda_home/lib64"
    elif [ -d "$cuda_home/lib" ]; then
        printf 'export LD_LIBRARY_PATH=%q:"${LD_LIBRARY_PATH:-}"\n' "$cuda_home/lib"
    fi
    exit 0
fi

if [ "$want_cmake" -eq 1 ]; then
    if [ -z "$cuda_home" ]; then
        echo "CUDA toolkit was not found; cannot generate CUDA CMake command." >&2
        exit 1
    fi

    printf 'cmake -S . -B build-cuda -DSTIPPLE_ENABLE_CUDA=ON -DCMAKE_CUDA_COMPILER=%q -DCUDAToolkit_ROOT=%q\n' "$nvcc_path" "$cuda_home"
    exit 0
fi

echo "CUDA environment check"
echo "======================"

if [ -n "$cuda_home" ]; then
    print_status "CUDA_HOME inferred:" "$cuda_home"
else
    print_status "CUDA_HOME inferred:" "not found"
fi

if [ -n "$nvcc_path" ]; then
    print_status "nvcc:" "$nvcc_path"
    echo
    "$nvcc_path" --version || true
else
    print_status "nvcc:" "not found"
fi

echo
if command_exists nvidia-smi; then
    print_status "nvidia-smi:" "found"
    if nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv,noheader; then
        cuda_runtime_available=1
    else
        cuda_runtime_available=0
    fi
else
    print_status "nvidia-smi:" "not found"
fi

echo
if [ "$cuda_available" -eq 1 ]; then
    print_status "Toolkit result:" "available"
    echo
    echo "Configure this project with:"
    printf '  cmake -S . -B build-cuda -DSTIPPLE_ENABLE_CUDA=ON -DCMAKE_CUDA_COMPILER=%q -DCUDAToolkit_ROOT=%q\n' "$nvcc_path" "$cuda_home"
    echo "  cmake --build build-cuda"
else
    print_status "Toolkit result:" "not available: nvcc was not found"
    echo
    echo "Install the CUDA Toolkit or make nvcc visible in PATH."
    echo "If CUDA is installed in a custom location, run something like:"
    echo "  export CUDA_HOME=/path/to/cuda"
    echo "  export CUDAToolkit_ROOT=/path/to/cuda"
    echo "  export PATH=/path/to/cuda/bin:\"\$PATH\""
    echo "  export LD_LIBRARY_PATH=/path/to/cuda/lib64:\"\${LD_LIBRARY_PATH:-}\""
fi

echo
if [ "$cuda_runtime_available" -eq 1 ]; then
    print_status "Runtime result:" "available: NVIDIA driver can see a CUDA GPU"
else
    print_status "Runtime result:" "not available: NVIDIA driver/GPU is not working or not visible"
    echo
    echo "CUDA code may compile, but CUDA mode cannot run until nvidia-smi works."
fi
