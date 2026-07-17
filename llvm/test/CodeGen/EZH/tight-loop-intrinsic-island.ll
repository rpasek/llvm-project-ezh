; RUN: llc -mtriple=ezh-none-elf -O2 %s -o - | FileCheck %s
;
; A hand-written __builtin_ezh_tight_loop imposes the same layout contract as
; a compiler-formed one: the hardware repeats every instruction from after the
; run-once slot up to the Rend address (the blockaddress label). The constant
; island pass must therefore never create water at the end of a block that
; contains a TIGHT_LOOP -- an island plus its branch-around goto would land
; inside the repeated region, and the taken goto aborts the hardware loop
; (observed on silicon: a one-instruction pump body ran exactly once per
; entry). The pool pressure below forces createNewWater at the loop block;
; the island must be placed BEFORE the loop, leaving the body directly
; adjacent to its Rend label.
;
; Between tight_loop and the .Ltmp Rend label there must be no island data
; and no branch-around goto. (An IR-level operand materialization may still
; sit inside the region -- a hand-code hazard outside this pass's control --
; so the checks pin only what the island pass owns.)
; CHECK-LABEL: f:
; CHECK: tight_loop
; CHECK-NOT: .LCPI
; CHECK-NOT: goto
; CHECK: .Ltmp0:

define void @f(ptr %p, ptr %q, i32 %n) {
entry:
  %c = icmp eq i32 %n, 0
  br i1 %c, label %ex, label %loop

loop:
  call void @llvm.ezh.tight.loop(ptr blockaddress(@f, %ex), i32 1999)
  call void asm sideeffect "nop", ""()
  %r = call ptr asm sideeffect "str_post $0, $1, 4", "=r,r,0,~{memory}"(i32 0, ptr %p)
  br label %ex

ex:
  %g0 = getelementptr i32, ptr %q, i32 0
  store volatile i32 1073741825, ptr %g0
  %g1 = getelementptr i32, ptr %q, i32 1
  store volatile i32 1090584835, ptr %g1
  %g2 = getelementptr i32, ptr %q, i32 2
  store volatile i32 1107427845, ptr %g2
  %g3 = getelementptr i32, ptr %q, i32 3
  store volatile i32 1124270855, ptr %g3
  %g4 = getelementptr i32, ptr %q, i32 4
  store volatile i32 1141113865, ptr %g4
  %g5 = getelementptr i32, ptr %q, i32 5
  store volatile i32 1157956875, ptr %g5
  %g6 = getelementptr i32, ptr %q, i32 6
  store volatile i32 1174799885, ptr %g6
  %g7 = getelementptr i32, ptr %q, i32 7
  store volatile i32 1191642895, ptr %g7
  %g8 = getelementptr i32, ptr %q, i32 8
  store volatile i32 1208485905, ptr %g8
  %g9 = getelementptr i32, ptr %q, i32 9
  store volatile i32 1225328915, ptr %g9
  %g10 = getelementptr i32, ptr %q, i32 10
  store volatile i32 1242171925, ptr %g10
  %g11 = getelementptr i32, ptr %q, i32 11
  store volatile i32 1259014935, ptr %g11
  %g12 = getelementptr i32, ptr %q, i32 12
  store volatile i32 1275857945, ptr %g12
  %g13 = getelementptr i32, ptr %q, i32 13
  store volatile i32 1292700955, ptr %g13
  %g14 = getelementptr i32, ptr %q, i32 14
  store volatile i32 1309543965, ptr %g14
  %g15 = getelementptr i32, ptr %q, i32 15
  store volatile i32 1326386975, ptr %g15
  %g16 = getelementptr i32, ptr %q, i32 16
  store volatile i32 1343229985, ptr %g16
  %g17 = getelementptr i32, ptr %q, i32 17
  store volatile i32 1360072995, ptr %g17
  %g18 = getelementptr i32, ptr %q, i32 18
  store volatile i32 1376916005, ptr %g18
  %g19 = getelementptr i32, ptr %q, i32 19
  store volatile i32 1393759015, ptr %g19
  %g20 = getelementptr i32, ptr %q, i32 20
  store volatile i32 1410602025, ptr %g20
  %g21 = getelementptr i32, ptr %q, i32 21
  store volatile i32 1427445035, ptr %g21
  %g22 = getelementptr i32, ptr %q, i32 22
  store volatile i32 1444288045, ptr %g22
  %g23 = getelementptr i32, ptr %q, i32 23
  store volatile i32 1461131055, ptr %g23
  %g24 = getelementptr i32, ptr %q, i32 24
  store volatile i32 1477974065, ptr %g24
  %g25 = getelementptr i32, ptr %q, i32 25
  store volatile i32 1494817075, ptr %g25
  %g26 = getelementptr i32, ptr %q, i32 26
  store volatile i32 1511660085, ptr %g26
  %g27 = getelementptr i32, ptr %q, i32 27
  store volatile i32 1528503095, ptr %g27
  %g28 = getelementptr i32, ptr %q, i32 28
  store volatile i32 1545346105, ptr %g28
  %g29 = getelementptr i32, ptr %q, i32 29
  store volatile i32 1562189115, ptr %g29
  %g30 = getelementptr i32, ptr %q, i32 30
  store volatile i32 1579032125, ptr %g30
  %g31 = getelementptr i32, ptr %q, i32 31
  store volatile i32 1595875135, ptr %g31
  %g32 = getelementptr i32, ptr %q, i32 32
  store volatile i32 1612718145, ptr %g32
  %g33 = getelementptr i32, ptr %q, i32 33
  store volatile i32 1629561155, ptr %g33
  %g34 = getelementptr i32, ptr %q, i32 34
  store volatile i32 1646404165, ptr %g34
  %g35 = getelementptr i32, ptr %q, i32 35
  store volatile i32 1663247175, ptr %g35
  %g36 = getelementptr i32, ptr %q, i32 36
  store volatile i32 1680090185, ptr %g36
  %g37 = getelementptr i32, ptr %q, i32 37
  store volatile i32 1696933195, ptr %g37
  %g38 = getelementptr i32, ptr %q, i32 38
  store volatile i32 1713776205, ptr %g38
  %g39 = getelementptr i32, ptr %q, i32 39
  store volatile i32 1730619215, ptr %g39
  %g40 = getelementptr i32, ptr %q, i32 40
  store volatile i32 1747462225, ptr %g40
  %g41 = getelementptr i32, ptr %q, i32 41
  store volatile i32 1764305235, ptr %g41
  %g42 = getelementptr i32, ptr %q, i32 42
  store volatile i32 1781148245, ptr %g42
  %g43 = getelementptr i32, ptr %q, i32 43
  store volatile i32 1797991255, ptr %g43
  %g44 = getelementptr i32, ptr %q, i32 44
  store volatile i32 1814834265, ptr %g44
  %g45 = getelementptr i32, ptr %q, i32 45
  store volatile i32 1831677275, ptr %g45
  %g46 = getelementptr i32, ptr %q, i32 46
  store volatile i32 1848520285, ptr %g46
  %g47 = getelementptr i32, ptr %q, i32 47
  store volatile i32 1865363295, ptr %g47
  %g48 = getelementptr i32, ptr %q, i32 48
  store volatile i32 1882206305, ptr %g48
  %g49 = getelementptr i32, ptr %q, i32 49
  store volatile i32 1899049315, ptr %g49
  %g50 = getelementptr i32, ptr %q, i32 50
  store volatile i32 1915892325, ptr %g50
  %g51 = getelementptr i32, ptr %q, i32 51
  store volatile i32 1932735335, ptr %g51
  %g52 = getelementptr i32, ptr %q, i32 52
  store volatile i32 1949578345, ptr %g52
  %g53 = getelementptr i32, ptr %q, i32 53
  store volatile i32 1966421355, ptr %g53
  %g54 = getelementptr i32, ptr %q, i32 54
  store volatile i32 1983264365, ptr %g54
  %g55 = getelementptr i32, ptr %q, i32 55
  store volatile i32 2000107375, ptr %g55
  %g56 = getelementptr i32, ptr %q, i32 56
  store volatile i32 2016950385, ptr %g56
  %g57 = getelementptr i32, ptr %q, i32 57
  store volatile i32 2033793395, ptr %g57
  %g58 = getelementptr i32, ptr %q, i32 58
  store volatile i32 2050636405, ptr %g58
  %g59 = getelementptr i32, ptr %q, i32 59
  store volatile i32 2067479415, ptr %g59
  %g60 = getelementptr i32, ptr %q, i32 60
  store volatile i32 2084322425, ptr %g60
  %g61 = getelementptr i32, ptr %q, i32 61
  store volatile i32 2101165435, ptr %g61
  %g62 = getelementptr i32, ptr %q, i32 62
  store volatile i32 2118008445, ptr %g62
  %g63 = getelementptr i32, ptr %q, i32 63
  store volatile i32 2134851455, ptr %g63
  %g64 = getelementptr i32, ptr %q, i32 64
  store volatile i32 2151694465, ptr %g64
  %g65 = getelementptr i32, ptr %q, i32 65
  store volatile i32 2168537475, ptr %g65
  %g66 = getelementptr i32, ptr %q, i32 66
  store volatile i32 2185380485, ptr %g66
  %g67 = getelementptr i32, ptr %q, i32 67
  store volatile i32 2202223495, ptr %g67
  %g68 = getelementptr i32, ptr %q, i32 68
  store volatile i32 2219066505, ptr %g68
  %g69 = getelementptr i32, ptr %q, i32 69
  store volatile i32 2235909515, ptr %g69
  %g70 = getelementptr i32, ptr %q, i32 70
  store volatile i32 2252752525, ptr %g70
  %g71 = getelementptr i32, ptr %q, i32 71
  store volatile i32 2269595535, ptr %g71
  %g72 = getelementptr i32, ptr %q, i32 72
  store volatile i32 2286438545, ptr %g72
  %g73 = getelementptr i32, ptr %q, i32 73
  store volatile i32 2303281555, ptr %g73
  %g74 = getelementptr i32, ptr %q, i32 74
  store volatile i32 2320124565, ptr %g74
  %g75 = getelementptr i32, ptr %q, i32 75
  store volatile i32 2336967575, ptr %g75
  %g76 = getelementptr i32, ptr %q, i32 76
  store volatile i32 2353810585, ptr %g76
  %g77 = getelementptr i32, ptr %q, i32 77
  store volatile i32 2370653595, ptr %g77
  %g78 = getelementptr i32, ptr %q, i32 78
  store volatile i32 2387496605, ptr %g78
  %g79 = getelementptr i32, ptr %q, i32 79
  store volatile i32 2404339615, ptr %g79
  %g80 = getelementptr i32, ptr %q, i32 80
  store volatile i32 2421182625, ptr %g80
  %g81 = getelementptr i32, ptr %q, i32 81
  store volatile i32 2438025635, ptr %g81
  %g82 = getelementptr i32, ptr %q, i32 82
  store volatile i32 2454868645, ptr %g82
  %g83 = getelementptr i32, ptr %q, i32 83
  store volatile i32 2471711655, ptr %g83
  %g84 = getelementptr i32, ptr %q, i32 84
  store volatile i32 2488554665, ptr %g84
  %g85 = getelementptr i32, ptr %q, i32 85
  store volatile i32 2505397675, ptr %g85
  %g86 = getelementptr i32, ptr %q, i32 86
  store volatile i32 2522240685, ptr %g86
  %g87 = getelementptr i32, ptr %q, i32 87
  store volatile i32 2539083695, ptr %g87
  %g88 = getelementptr i32, ptr %q, i32 88
  store volatile i32 2555926705, ptr %g88
  %g89 = getelementptr i32, ptr %q, i32 89
  store volatile i32 2572769715, ptr %g89
  %g90 = getelementptr i32, ptr %q, i32 90
  store volatile i32 2589612725, ptr %g90
  %g91 = getelementptr i32, ptr %q, i32 91
  store volatile i32 2606455735, ptr %g91
  %g92 = getelementptr i32, ptr %q, i32 92
  store volatile i32 2623298745, ptr %g92
  %g93 = getelementptr i32, ptr %q, i32 93
  store volatile i32 2640141755, ptr %g93
  %g94 = getelementptr i32, ptr %q, i32 94
  store volatile i32 2656984765, ptr %g94
  %g95 = getelementptr i32, ptr %q, i32 95
  store volatile i32 2673827775, ptr %g95
  %g96 = getelementptr i32, ptr %q, i32 96
  store volatile i32 2690670785, ptr %g96
  %g97 = getelementptr i32, ptr %q, i32 97
  store volatile i32 2707513795, ptr %g97
  %g98 = getelementptr i32, ptr %q, i32 98
  store volatile i32 2724356805, ptr %g98
  %g99 = getelementptr i32, ptr %q, i32 99
  store volatile i32 2741199815, ptr %g99
  %g100 = getelementptr i32, ptr %q, i32 100
  store volatile i32 2758042825, ptr %g100
  %g101 = getelementptr i32, ptr %q, i32 101
  store volatile i32 2774885835, ptr %g101
  %g102 = getelementptr i32, ptr %q, i32 102
  store volatile i32 2791728845, ptr %g102
  %g103 = getelementptr i32, ptr %q, i32 103
  store volatile i32 2808571855, ptr %g103
  %g104 = getelementptr i32, ptr %q, i32 104
  store volatile i32 2825414865, ptr %g104
  %g105 = getelementptr i32, ptr %q, i32 105
  store volatile i32 2842257875, ptr %g105
  %g106 = getelementptr i32, ptr %q, i32 106
  store volatile i32 2859100885, ptr %g106
  %g107 = getelementptr i32, ptr %q, i32 107
  store volatile i32 2875943895, ptr %g107
  %g108 = getelementptr i32, ptr %q, i32 108
  store volatile i32 2892786905, ptr %g108
  %g109 = getelementptr i32, ptr %q, i32 109
  store volatile i32 2909629915, ptr %g109
  %g110 = getelementptr i32, ptr %q, i32 110
  store volatile i32 2926472925, ptr %g110
  %g111 = getelementptr i32, ptr %q, i32 111
  store volatile i32 2943315935, ptr %g111
  %g112 = getelementptr i32, ptr %q, i32 112
  store volatile i32 2960158945, ptr %g112
  %g113 = getelementptr i32, ptr %q, i32 113
  store volatile i32 2977001955, ptr %g113
  %g114 = getelementptr i32, ptr %q, i32 114
  store volatile i32 2993844965, ptr %g114
  %g115 = getelementptr i32, ptr %q, i32 115
  store volatile i32 3010687975, ptr %g115
  %g116 = getelementptr i32, ptr %q, i32 116
  store volatile i32 3027530985, ptr %g116
  %g117 = getelementptr i32, ptr %q, i32 117
  store volatile i32 3044373995, ptr %g117
  %g118 = getelementptr i32, ptr %q, i32 118
  store volatile i32 3061217005, ptr %g118
  %g119 = getelementptr i32, ptr %q, i32 119
  store volatile i32 3078060015, ptr %g119
  ret void
}

declare void @llvm.ezh.tight.loop(ptr, i32)
