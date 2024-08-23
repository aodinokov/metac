This is an alternative approach to the c_print_args

The original approach requires some non-documented capabilities of va_arg to work with structures and
va_list (it's not easy to extract struct or va_list from va_list which we used as a storage).
The current approach in its turn requires __typeof compiler extension.
Also this apporach has a limitation of number of arguments: it's necessary to regenerate mr_pp.generated.h using mr_pp.sh and provide the maximum number of arguments. For now it's generated with maximum 1024 arguments
which is mentioned as a minimum supported number of arguments in C in some referenses. 
