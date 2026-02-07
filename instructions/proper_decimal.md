
add a compile time switch to have 3 versions of the decimal type

- the current one
- a crappy one using double as the underlying representation
- something built using https://github.com/ckormanyos/wide-integer/blob/master/math/wide_integer/uintwide_t.h with a fixed version,
where mul/dive are done with wider integers (at least 196 or 256 bits), then cast'd back to 128 with the right scaling
- btw write somewhare how many bits for 10^18 

make sure all numerical types are tested whatever the build config ...
having the test being a sequence of 
test<decimal_type1>();
test<decimal_type2>();
test<decimal_type3>();

is fine, or maybe separate for easier debug 

