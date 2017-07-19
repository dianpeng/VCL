#/bin/sh

options=""

if [ -f "./.clang-format" ]; then
	options="--assume-filename=./.clang-format"
else
	options="--style=Google"
fi

find include/ -iname *.h -o -iname *.cc | xargs clang-format -i $options
find src/ -name *.h -o -iname *.cc | xargs clang-format -i $options

# ====================================================
# *DO NOT* run clang-format in test/ folder.
# It will mess up inline VCL code
# ====================================================
