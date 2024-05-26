This is example and test simultaniously. We're checking that:
1. the app can have several declarations of the type with the same name, but with the different internal data (struct dup)
2. the app can have several object files with static variables and functions which have the same name. Also one more module may have a function or variable with the same name as non-static
3. the function can have several local variables with the same name in different visibility scope
4. and metadb has to handle both things correctly

as a result we've got:
```
$ nm _meta_c_app_complex 
000000000000038c r __abi_tag
0000000000004028 B __bss_start
0000000000004028 b completed.0
                 w __cxa_finalize@GLIBC_2.2.5
0000000000004010 d data
0000000000004014 d data
0000000000004020 d data
0000000000004018 D data
0000000000004000 D __data_start
0000000000004000 W data_start
0000000000001090 t deregister_tm_clones
0000000000001100 t __do_global_dtors_aux
0000000000003dc0 d __do_global_dtors_aux_fini_array_entry
0000000000004008 D __dso_handle
0000000000003dc8 d _DYNAMIC
0000000000004028 D _edata
0000000000004030 B _end
00000000000012a0 T _fini
0000000000001140 t frame_dummy
0000000000003db8 d __frame_dummy_init_array_entry
0000000000002280 r __FRAME_END__
0000000000003fb8 d _GLOBAL_OFFSET_TABLE_
                 w __gmon_start__
0000000000002060 r __GNU_EH_FRAME_HDR
0000000000001000 T _init
0000000000002000 R _IO_stdin_used
                 w _ITM_deregisterTMCloneTable
                 w _ITM_registerTMCloneTable
                 U __libc_start_main@GLIBC_2.34
0000000000001149 T main
0000000000001180 t _module1
00000000000011ba t _module1
00000000000011f4 T _module1
00000000000011a6 T module1f1
00000000000011e0 T module1f2
000000000000121c T module2
0000000000001230 t _module3
000000000000128a T module3
                 U printf@GLIBC_2.2.5
00000000000010c0 t register_tm_clones
0000000000001060 T _start
0000000000004028 D __TMC_END__
0000000000004024 D uniq

```