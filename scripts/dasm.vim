" Vim syntax file
" Language: uJIT DynaASM
" Maintainer: Igor Ehrlich
" Latest Revision: 16 June 2015

if exists("b:current_syntax")
  finish
endif

syn keyword dDefine saveregs_ saveregs restoreregs i2tvp i2gcr i2gct gcr2i gct2i ngct2i gco2i num2i checktag gettag settag ptrdiff2nargs inc_PC save_PC restore_PC i2ftsz i2func ftsz2offs setup_dispatch redispatch_static setup_kbase ins_A ins_AD ins_AJ ins_ABC ins_AB_ ins_A_C ins_AND _ins_next ins_next ins_callt ins_call kitva2r iuva2r tbl_check_mm tbl_find_key mov64rr movtv checktp checknum checkstr checktab checkfunc checknotnil _checksealed checksealed_bc checksealed_ff branchPC hotloop hotcall set_vmstate fcomparepp fdup fpop1 sseconst_abs sseconst_hi sseconst_sign sseconst_1 sseconst_m1 sseconst_2p52 sseconst_tobit barrierback ffgccheck restore_RA restore_RB restore_RC restore_RD
syn keyword dInstr  mov add shl lea sub shr movzx jmp test jz jnz and imul cmp jp je jne jb jbe ja jae fucomip fpop mov64 neg xor ret call movsxd cvtsi2sd movsd fld sar not cmova addsd cvtsd2si xorps mulsd divsd or subsd cvtsi2sd jl jle jns push pop ucomisd cvttsd2si movd int3 js adc movsx jnb dec jg movaps movdqu
syn keyword dType byte word dword qword
syn keyword dConst LJ_TSTR LJ_TNIL LJ_TISNUM LJ_TTAB LJ_TFUNC LJ_TTHREAD LJ_GC_BLACK LJ_TTRUE LJ_TFALSE LJ_TISTRUECOND LJ_TNUMX LJ_TLIGHTUD LJ_TUDATA LJ_TCDATA LJ_TISPRI LJ_TISTABUD LJ_TISGCV LJ_TNUMBER LJ_GC_WHITES FRAME_VARG FRAME_P FRAME_TYPE FRAME_C CFRAME_RAWMASK CFRAME_OFS_L CFRAME_OFS_PC LUA_YIELD LUA_MINSTACK FRAME_CP CFRAME_RESUME FRAME_CONT FRAME_PCALL LJ_ERR_SEAL_CONST LJ_GC_SEALED
syn keyword dDwordReg KBASEd RA RB RC RD DISPATCHd XCHGd AUX1d AUX2d CARG1d CARG2d CARG3d dword

syn region dComment start="//" end="$"
syn match dLabel "->[a-zA-Z0-9@_]\+:"

let b:current_syntax = "dasm"

hi def link dComment  Comment
hi def link dDefine   PreProc
hi def link dInstr    Type
hi def link dType     Type
hi def link dConst    Constant
hi def link dLabel    Statement
hi def link dDwordReg Error
