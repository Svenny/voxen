set -o errexit

build_dir="../build"
cmake_cmd="cmake"
num_jobs=16

while [[ -n "$1" ]]; do case $1 in
	-h | --help )
		echo "Usage: build.sh --build-dir <dir> --cmake-cmd <path> [-j <n>] [-- <CMake args>]"
		exit 0
		;;
	--build-dir )
		shift
		build_dir="$1"
		;;
	--cmake-cmd )
		shift
		cmake_cmd="$1"
		;;
	-j )
		shift
		if (( $1 >= 1 )); then
			num_jobs=$1
		else
			echo "Invalid number of jobs ($1), aborting"
			exit 1
		fi
		;;
	-- )
		# Next arguments will be CMake parameters
		shift
		break
		;;
	* )
		echo "Unknown argument $1, aborting"
		exit 1
		;;
esac; shift; done

if [[ -z "$build_dir" ]]; then
	echo "No build directory provided, aborting"
	exit 1
fi
case $build_dir in
	/* ) build_dir=`readlink -f $build_dir`;;
	* ) build_dir=`readlink -f ${0%/*}/$build_dir`;;
esac
if [[ ! -d "$build_dir" ]]; then
	echo "$build_dir is not an existing directory, aborting"
	exit 1
fi

if [[ -z "$cmake_cmd" ]]; then
	echo "No CMake binary provided, aborting"
	exit 1
fi

source_dir=`readlink -f "${0%/*}/.."`
install_dir="$build_dir/3rdparty"
build_dir="$build_dir/3rdparty-build"
cmake_args="$@"

echo "Source dir is $source_dir"
echo "Building in $build_dir"
echo "Installing in $install_dir"
echo "Using CMake command $cmake_cmd"
echo "Using up to $num_jobs parallel jobs"
echo "Additional CMake arguments: $cmake_args"

mkdir -p $install_dir

# FMT

fmt_args="-DFMT_DOC=OFF -DFMT_TEST=OFF -DFMT_FUZZ=OFF -DFMT_CUDA_TEST=OFF"
fmt_dir="$build_dir/fmt"
mkdir -p $fmt_dir
(cd $fmt_dir && $cmake_cmd $fmt_args -DCMAKE_INSTALL_PREFIX=$install_dir $cmake_args "$source_dir/3rdparty/fmt" && $cmake_cmd --build . -j $num_jobs --target install)

exit 0
