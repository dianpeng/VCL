#/bin/sh
find include/ -iname *.h -o -iname *.cc | xargs clang-format -i --style=Google
find src/ -name *.h -o -iname *.cc | xargs clang-format -i --style=Google

# ====================================================
# *DO NOT* run clang-format in test/ folder.
# It will mess up inline VCL code
# ====================================================
