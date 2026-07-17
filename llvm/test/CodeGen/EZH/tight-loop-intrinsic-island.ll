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
; CHECK-NOT: .LCPI{{[0-9_]+}}:
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

; The repeated region spans TWO fall-through blocks (%loop, %mid) before the
; Rend label, with pool-free filler stranding %mid's own pool user on both
; sides. This locks the composite protection for multi-block regions built
; from ordinary IR: Rend resolves from the loop block's own constant-pool
; load, %mid is marked, and any water needed by an in-region user is created
; before the region. (In this shape the loop block's Rend load is processed
; first and its guarded island also shields the region; the walk itself is
; exercised directly, without that shield, by the unresolved-Rend MIR test
; tight-loop-region-water.mir.)
; CHECK-LABEL: g:
; CHECK: tight_loop
; CHECK-NOT: .LCPI{{[0-9_]+}}:
; CHECK-NOT: goto
; CHECK: .Ltmp1:

define void @g(ptr %p, ptr %q, i32 %n) {
entry:
  store volatile ptr blockaddress(@g, %ex2), ptr %q
  store volatile ptr blockaddress(@g, %mid), ptr %q
  %c = icmp eq i32 %n, 0
  br i1 %c, label %f1blk, label %loop

f1blk:
  %f0 = getelementptr i32, ptr %q, i32 0
  store volatile i32 3, ptr %f0
  %f1 = getelementptr i32, ptr %q, i32 1
  store volatile i32 4, ptr %f1
  %f2 = getelementptr i32, ptr %q, i32 2
  store volatile i32 5, ptr %f2
  %f3 = getelementptr i32, ptr %q, i32 3
  store volatile i32 6, ptr %f3
  %f4 = getelementptr i32, ptr %q, i32 4
  store volatile i32 7, ptr %f4
  %f5 = getelementptr i32, ptr %q, i32 5
  store volatile i32 3, ptr %f5
  %f6 = getelementptr i32, ptr %q, i32 6
  store volatile i32 4, ptr %f6
  %f7 = getelementptr i32, ptr %q, i32 7
  store volatile i32 5, ptr %f7
  %f8 = getelementptr i32, ptr %q, i32 8
  store volatile i32 6, ptr %f8
  %f9 = getelementptr i32, ptr %q, i32 9
  store volatile i32 7, ptr %f9
  %f10 = getelementptr i32, ptr %q, i32 10
  store volatile i32 3, ptr %f10
  %f11 = getelementptr i32, ptr %q, i32 11
  store volatile i32 4, ptr %f11
  %f12 = getelementptr i32, ptr %q, i32 12
  store volatile i32 5, ptr %f12
  %f13 = getelementptr i32, ptr %q, i32 13
  store volatile i32 6, ptr %f13
  %f14 = getelementptr i32, ptr %q, i32 14
  store volatile i32 7, ptr %f14
  %f15 = getelementptr i32, ptr %q, i32 15
  store volatile i32 3, ptr %f15
  %f16 = getelementptr i32, ptr %q, i32 16
  store volatile i32 4, ptr %f16
  %f17 = getelementptr i32, ptr %q, i32 17
  store volatile i32 5, ptr %f17
  %f18 = getelementptr i32, ptr %q, i32 18
  store volatile i32 6, ptr %f18
  %f19 = getelementptr i32, ptr %q, i32 19
  store volatile i32 7, ptr %f19
  %f20 = getelementptr i32, ptr %q, i32 20
  store volatile i32 3, ptr %f20
  %f21 = getelementptr i32, ptr %q, i32 21
  store volatile i32 4, ptr %f21
  %f22 = getelementptr i32, ptr %q, i32 22
  store volatile i32 5, ptr %f22
  %f23 = getelementptr i32, ptr %q, i32 23
  store volatile i32 6, ptr %f23
  %f24 = getelementptr i32, ptr %q, i32 24
  store volatile i32 7, ptr %f24
  %f25 = getelementptr i32, ptr %q, i32 25
  store volatile i32 3, ptr %f25
  %f26 = getelementptr i32, ptr %q, i32 26
  store volatile i32 4, ptr %f26
  %f27 = getelementptr i32, ptr %q, i32 27
  store volatile i32 5, ptr %f27
  %f28 = getelementptr i32, ptr %q, i32 28
  store volatile i32 6, ptr %f28
  %f29 = getelementptr i32, ptr %q, i32 29
  store volatile i32 7, ptr %f29
  %f30 = getelementptr i32, ptr %q, i32 30
  store volatile i32 3, ptr %f30
  %f31 = getelementptr i32, ptr %q, i32 31
  store volatile i32 4, ptr %f31
  %f32 = getelementptr i32, ptr %q, i32 32
  store volatile i32 5, ptr %f32
  %f33 = getelementptr i32, ptr %q, i32 33
  store volatile i32 6, ptr %f33
  %f34 = getelementptr i32, ptr %q, i32 34
  store volatile i32 7, ptr %f34
  %f35 = getelementptr i32, ptr %q, i32 35
  store volatile i32 3, ptr %f35
  %f36 = getelementptr i32, ptr %q, i32 36
  store volatile i32 4, ptr %f36
  %f37 = getelementptr i32, ptr %q, i32 37
  store volatile i32 5, ptr %f37
  %f38 = getelementptr i32, ptr %q, i32 38
  store volatile i32 6, ptr %f38
  %f39 = getelementptr i32, ptr %q, i32 39
  store volatile i32 7, ptr %f39
  %f40 = getelementptr i32, ptr %q, i32 40
  store volatile i32 3, ptr %f40
  %f41 = getelementptr i32, ptr %q, i32 41
  store volatile i32 4, ptr %f41
  %f42 = getelementptr i32, ptr %q, i32 42
  store volatile i32 5, ptr %f42
  %f43 = getelementptr i32, ptr %q, i32 43
  store volatile i32 6, ptr %f43
  %f44 = getelementptr i32, ptr %q, i32 44
  store volatile i32 7, ptr %f44
  %f45 = getelementptr i32, ptr %q, i32 45
  store volatile i32 3, ptr %f45
  %f46 = getelementptr i32, ptr %q, i32 46
  store volatile i32 4, ptr %f46
  %f47 = getelementptr i32, ptr %q, i32 47
  store volatile i32 5, ptr %f47
  %f48 = getelementptr i32, ptr %q, i32 48
  store volatile i32 6, ptr %f48
  %f49 = getelementptr i32, ptr %q, i32 49
  store volatile i32 7, ptr %f49
  %f50 = getelementptr i32, ptr %q, i32 50
  store volatile i32 3, ptr %f50
  %f51 = getelementptr i32, ptr %q, i32 51
  store volatile i32 4, ptr %f51
  %f52 = getelementptr i32, ptr %q, i32 52
  store volatile i32 5, ptr %f52
  %f53 = getelementptr i32, ptr %q, i32 53
  store volatile i32 6, ptr %f53
  %f54 = getelementptr i32, ptr %q, i32 54
  store volatile i32 7, ptr %f54
  %f55 = getelementptr i32, ptr %q, i32 55
  store volatile i32 3, ptr %f55
  %f56 = getelementptr i32, ptr %q, i32 56
  store volatile i32 4, ptr %f56
  %f57 = getelementptr i32, ptr %q, i32 57
  store volatile i32 5, ptr %f57
  %f58 = getelementptr i32, ptr %q, i32 58
  store volatile i32 6, ptr %f58
  %f59 = getelementptr i32, ptr %q, i32 59
  store volatile i32 7, ptr %f59
  %f60 = getelementptr i32, ptr %q, i32 60
  store volatile i32 3, ptr %f60
  %f61 = getelementptr i32, ptr %q, i32 61
  store volatile i32 4, ptr %f61
  %f62 = getelementptr i32, ptr %q, i32 62
  store volatile i32 5, ptr %f62
  %f63 = getelementptr i32, ptr %q, i32 63
  store volatile i32 6, ptr %f63
  %f64 = getelementptr i32, ptr %q, i32 64
  store volatile i32 7, ptr %f64
  %f65 = getelementptr i32, ptr %q, i32 65
  store volatile i32 3, ptr %f65
  %f66 = getelementptr i32, ptr %q, i32 66
  store volatile i32 4, ptr %f66
  %f67 = getelementptr i32, ptr %q, i32 67
  store volatile i32 5, ptr %f67
  %f68 = getelementptr i32, ptr %q, i32 68
  store volatile i32 6, ptr %f68
  %f69 = getelementptr i32, ptr %q, i32 69
  store volatile i32 7, ptr %f69
  %f70 = getelementptr i32, ptr %q, i32 70
  store volatile i32 3, ptr %f70
  %f71 = getelementptr i32, ptr %q, i32 71
  store volatile i32 4, ptr %f71
  %f72 = getelementptr i32, ptr %q, i32 72
  store volatile i32 5, ptr %f72
  %f73 = getelementptr i32, ptr %q, i32 73
  store volatile i32 6, ptr %f73
  %f74 = getelementptr i32, ptr %q, i32 74
  store volatile i32 7, ptr %f74
  %f75 = getelementptr i32, ptr %q, i32 75
  store volatile i32 3, ptr %f75
  %f76 = getelementptr i32, ptr %q, i32 76
  store volatile i32 4, ptr %f76
  %f77 = getelementptr i32, ptr %q, i32 77
  store volatile i32 5, ptr %f77
  %f78 = getelementptr i32, ptr %q, i32 78
  store volatile i32 6, ptr %f78
  %f79 = getelementptr i32, ptr %q, i32 79
  store volatile i32 7, ptr %f79
  %f80 = getelementptr i32, ptr %q, i32 80
  store volatile i32 3, ptr %f80
  %f81 = getelementptr i32, ptr %q, i32 81
  store volatile i32 4, ptr %f81
  %f82 = getelementptr i32, ptr %q, i32 82
  store volatile i32 5, ptr %f82
  %f83 = getelementptr i32, ptr %q, i32 83
  store volatile i32 6, ptr %f83
  %f84 = getelementptr i32, ptr %q, i32 84
  store volatile i32 7, ptr %f84
  %f85 = getelementptr i32, ptr %q, i32 85
  store volatile i32 3, ptr %f85
  %f86 = getelementptr i32, ptr %q, i32 86
  store volatile i32 4, ptr %f86
  %f87 = getelementptr i32, ptr %q, i32 87
  store volatile i32 5, ptr %f87
  %f88 = getelementptr i32, ptr %q, i32 88
  store volatile i32 6, ptr %f88
  %f89 = getelementptr i32, ptr %q, i32 89
  store volatile i32 7, ptr %f89
  %f90 = getelementptr i32, ptr %q, i32 90
  store volatile i32 3, ptr %f90
  %f91 = getelementptr i32, ptr %q, i32 91
  store volatile i32 4, ptr %f91
  %f92 = getelementptr i32, ptr %q, i32 92
  store volatile i32 5, ptr %f92
  %f93 = getelementptr i32, ptr %q, i32 93
  store volatile i32 6, ptr %f93
  %f94 = getelementptr i32, ptr %q, i32 94
  store volatile i32 7, ptr %f94
  %f95 = getelementptr i32, ptr %q, i32 95
  store volatile i32 3, ptr %f95
  %f96 = getelementptr i32, ptr %q, i32 96
  store volatile i32 4, ptr %f96
  %f97 = getelementptr i32, ptr %q, i32 97
  store volatile i32 5, ptr %f97
  %f98 = getelementptr i32, ptr %q, i32 98
  store volatile i32 6, ptr %f98
  %f99 = getelementptr i32, ptr %q, i32 99
  store volatile i32 7, ptr %f99
  %f100 = getelementptr i32, ptr %q, i32 100
  store volatile i32 3, ptr %f100
  %f101 = getelementptr i32, ptr %q, i32 101
  store volatile i32 4, ptr %f101
  %f102 = getelementptr i32, ptr %q, i32 102
  store volatile i32 5, ptr %f102
  %f103 = getelementptr i32, ptr %q, i32 103
  store volatile i32 6, ptr %f103
  %f104 = getelementptr i32, ptr %q, i32 104
  store volatile i32 7, ptr %f104
  %f105 = getelementptr i32, ptr %q, i32 105
  store volatile i32 3, ptr %f105
  %f106 = getelementptr i32, ptr %q, i32 106
  store volatile i32 4, ptr %f106
  %f107 = getelementptr i32, ptr %q, i32 107
  store volatile i32 5, ptr %f107
  %f108 = getelementptr i32, ptr %q, i32 108
  store volatile i32 6, ptr %f108
  %f109 = getelementptr i32, ptr %q, i32 109
  store volatile i32 7, ptr %f109
  %f110 = getelementptr i32, ptr %q, i32 110
  store volatile i32 3, ptr %f110
  %f111 = getelementptr i32, ptr %q, i32 111
  store volatile i32 4, ptr %f111
  %f112 = getelementptr i32, ptr %q, i32 112
  store volatile i32 5, ptr %f112
  %f113 = getelementptr i32, ptr %q, i32 113
  store volatile i32 6, ptr %f113
  %f114 = getelementptr i32, ptr %q, i32 114
  store volatile i32 7, ptr %f114
  %f115 = getelementptr i32, ptr %q, i32 115
  store volatile i32 3, ptr %f115
  %f116 = getelementptr i32, ptr %q, i32 116
  store volatile i32 4, ptr %f116
  %f117 = getelementptr i32, ptr %q, i32 117
  store volatile i32 5, ptr %f117
  %f118 = getelementptr i32, ptr %q, i32 118
  store volatile i32 6, ptr %f118
  %f119 = getelementptr i32, ptr %q, i32 119
  store volatile i32 7, ptr %f119
  %f120 = getelementptr i32, ptr %q, i32 120
  store volatile i32 3, ptr %f120
  %f121 = getelementptr i32, ptr %q, i32 121
  store volatile i32 4, ptr %f121
  %f122 = getelementptr i32, ptr %q, i32 122
  store volatile i32 5, ptr %f122
  %f123 = getelementptr i32, ptr %q, i32 123
  store volatile i32 6, ptr %f123
  %f124 = getelementptr i32, ptr %q, i32 124
  store volatile i32 7, ptr %f124
  %f125 = getelementptr i32, ptr %q, i32 125
  store volatile i32 3, ptr %f125
  %f126 = getelementptr i32, ptr %q, i32 126
  store volatile i32 4, ptr %f126
  %f127 = getelementptr i32, ptr %q, i32 127
  store volatile i32 5, ptr %f127
  %f128 = getelementptr i32, ptr %q, i32 128
  store volatile i32 6, ptr %f128
  %f129 = getelementptr i32, ptr %q, i32 129
  store volatile i32 7, ptr %f129
  %f130 = getelementptr i32, ptr %q, i32 130
  store volatile i32 3, ptr %f130
  %f131 = getelementptr i32, ptr %q, i32 131
  store volatile i32 4, ptr %f131
  %f132 = getelementptr i32, ptr %q, i32 132
  store volatile i32 5, ptr %f132
  %f133 = getelementptr i32, ptr %q, i32 133
  store volatile i32 6, ptr %f133
  %f134 = getelementptr i32, ptr %q, i32 134
  store volatile i32 7, ptr %f134
  %f135 = getelementptr i32, ptr %q, i32 135
  store volatile i32 3, ptr %f135
  %f136 = getelementptr i32, ptr %q, i32 136
  store volatile i32 4, ptr %f136
  %f137 = getelementptr i32, ptr %q, i32 137
  store volatile i32 5, ptr %f137
  %f138 = getelementptr i32, ptr %q, i32 138
  store volatile i32 6, ptr %f138
  %f139 = getelementptr i32, ptr %q, i32 139
  store volatile i32 7, ptr %f139
  %f140 = getelementptr i32, ptr %q, i32 140
  store volatile i32 3, ptr %f140
  %f141 = getelementptr i32, ptr %q, i32 141
  store volatile i32 4, ptr %f141
  %f142 = getelementptr i32, ptr %q, i32 142
  store volatile i32 5, ptr %f142
  %f143 = getelementptr i32, ptr %q, i32 143
  store volatile i32 6, ptr %f143
  %f144 = getelementptr i32, ptr %q, i32 144
  store volatile i32 7, ptr %f144
  %f145 = getelementptr i32, ptr %q, i32 145
  store volatile i32 3, ptr %f145
  %f146 = getelementptr i32, ptr %q, i32 146
  store volatile i32 4, ptr %f146
  %f147 = getelementptr i32, ptr %q, i32 147
  store volatile i32 5, ptr %f147
  %f148 = getelementptr i32, ptr %q, i32 148
  store volatile i32 6, ptr %f148
  %f149 = getelementptr i32, ptr %q, i32 149
  store volatile i32 7, ptr %f149
  %f150 = getelementptr i32, ptr %q, i32 150
  store volatile i32 3, ptr %f150
  %f151 = getelementptr i32, ptr %q, i32 151
  store volatile i32 4, ptr %f151
  %f152 = getelementptr i32, ptr %q, i32 152
  store volatile i32 5, ptr %f152
  %f153 = getelementptr i32, ptr %q, i32 153
  store volatile i32 6, ptr %f153
  %f154 = getelementptr i32, ptr %q, i32 154
  store volatile i32 7, ptr %f154
  %f155 = getelementptr i32, ptr %q, i32 155
  store volatile i32 3, ptr %f155
  %f156 = getelementptr i32, ptr %q, i32 156
  store volatile i32 4, ptr %f156
  %f157 = getelementptr i32, ptr %q, i32 157
  store volatile i32 5, ptr %f157
  %f158 = getelementptr i32, ptr %q, i32 158
  store volatile i32 6, ptr %f158
  %f159 = getelementptr i32, ptr %q, i32 159
  store volatile i32 7, ptr %f159
  %f160 = getelementptr i32, ptr %q, i32 160
  store volatile i32 3, ptr %f160
  %f161 = getelementptr i32, ptr %q, i32 161
  store volatile i32 4, ptr %f161
  %f162 = getelementptr i32, ptr %q, i32 162
  store volatile i32 5, ptr %f162
  %f163 = getelementptr i32, ptr %q, i32 163
  store volatile i32 6, ptr %f163
  %f164 = getelementptr i32, ptr %q, i32 164
  store volatile i32 7, ptr %f164
  %f165 = getelementptr i32, ptr %q, i32 165
  store volatile i32 3, ptr %f165
  %f166 = getelementptr i32, ptr %q, i32 166
  store volatile i32 4, ptr %f166
  %f167 = getelementptr i32, ptr %q, i32 167
  store volatile i32 5, ptr %f167
  %f168 = getelementptr i32, ptr %q, i32 168
  store volatile i32 6, ptr %f168
  %f169 = getelementptr i32, ptr %q, i32 169
  store volatile i32 7, ptr %f169
  %f170 = getelementptr i32, ptr %q, i32 170
  store volatile i32 3, ptr %f170
  %f171 = getelementptr i32, ptr %q, i32 171
  store volatile i32 4, ptr %f171
  %f172 = getelementptr i32, ptr %q, i32 172
  store volatile i32 5, ptr %f172
  %f173 = getelementptr i32, ptr %q, i32 173
  store volatile i32 6, ptr %f173
  %f174 = getelementptr i32, ptr %q, i32 174
  store volatile i32 7, ptr %f174
  %f175 = getelementptr i32, ptr %q, i32 175
  store volatile i32 3, ptr %f175
  %f176 = getelementptr i32, ptr %q, i32 176
  store volatile i32 4, ptr %f176
  %f177 = getelementptr i32, ptr %q, i32 177
  store volatile i32 5, ptr %f177
  %f178 = getelementptr i32, ptr %q, i32 178
  store volatile i32 6, ptr %f178
  %f179 = getelementptr i32, ptr %q, i32 179
  store volatile i32 7, ptr %f179
  %f180 = getelementptr i32, ptr %q, i32 180
  store volatile i32 3, ptr %f180
  %f181 = getelementptr i32, ptr %q, i32 181
  store volatile i32 4, ptr %f181
  %f182 = getelementptr i32, ptr %q, i32 182
  store volatile i32 5, ptr %f182
  %f183 = getelementptr i32, ptr %q, i32 183
  store volatile i32 6, ptr %f183
  %f184 = getelementptr i32, ptr %q, i32 184
  store volatile i32 7, ptr %f184
  %f185 = getelementptr i32, ptr %q, i32 185
  store volatile i32 3, ptr %f185
  %f186 = getelementptr i32, ptr %q, i32 186
  store volatile i32 4, ptr %f186
  %f187 = getelementptr i32, ptr %q, i32 187
  store volatile i32 5, ptr %f187
  %f188 = getelementptr i32, ptr %q, i32 188
  store volatile i32 6, ptr %f188
  %f189 = getelementptr i32, ptr %q, i32 189
  store volatile i32 7, ptr %f189
  %f190 = getelementptr i32, ptr %q, i32 190
  store volatile i32 3, ptr %f190
  %f191 = getelementptr i32, ptr %q, i32 191
  store volatile i32 4, ptr %f191
  %f192 = getelementptr i32, ptr %q, i32 192
  store volatile i32 5, ptr %f192
  %f193 = getelementptr i32, ptr %q, i32 193
  store volatile i32 6, ptr %f193
  %f194 = getelementptr i32, ptr %q, i32 194
  store volatile i32 7, ptr %f194
  %f195 = getelementptr i32, ptr %q, i32 195
  store volatile i32 3, ptr %f195
  %f196 = getelementptr i32, ptr %q, i32 196
  store volatile i32 4, ptr %f196
  %f197 = getelementptr i32, ptr %q, i32 197
  store volatile i32 5, ptr %f197
  %f198 = getelementptr i32, ptr %q, i32 198
  store volatile i32 6, ptr %f198
  %f199 = getelementptr i32, ptr %q, i32 199
  store volatile i32 7, ptr %f199
  %f200 = getelementptr i32, ptr %q, i32 200
  store volatile i32 3, ptr %f200
  %f201 = getelementptr i32, ptr %q, i32 201
  store volatile i32 4, ptr %f201
  %f202 = getelementptr i32, ptr %q, i32 202
  store volatile i32 5, ptr %f202
  %f203 = getelementptr i32, ptr %q, i32 203
  store volatile i32 6, ptr %f203
  %f204 = getelementptr i32, ptr %q, i32 204
  store volatile i32 7, ptr %f204
  %f205 = getelementptr i32, ptr %q, i32 205
  store volatile i32 3, ptr %f205
  %f206 = getelementptr i32, ptr %q, i32 206
  store volatile i32 4, ptr %f206
  %f207 = getelementptr i32, ptr %q, i32 207
  store volatile i32 5, ptr %f207
  %f208 = getelementptr i32, ptr %q, i32 208
  store volatile i32 6, ptr %f208
  %f209 = getelementptr i32, ptr %q, i32 209
  store volatile i32 7, ptr %f209
  %f210 = getelementptr i32, ptr %q, i32 210
  store volatile i32 3, ptr %f210
  %f211 = getelementptr i32, ptr %q, i32 211
  store volatile i32 4, ptr %f211
  %f212 = getelementptr i32, ptr %q, i32 212
  store volatile i32 5, ptr %f212
  %f213 = getelementptr i32, ptr %q, i32 213
  store volatile i32 6, ptr %f213
  %f214 = getelementptr i32, ptr %q, i32 214
  store volatile i32 7, ptr %f214
  %f215 = getelementptr i32, ptr %q, i32 215
  store volatile i32 3, ptr %f215
  %f216 = getelementptr i32, ptr %q, i32 216
  store volatile i32 4, ptr %f216
  %f217 = getelementptr i32, ptr %q, i32 217
  store volatile i32 5, ptr %f217
  %f218 = getelementptr i32, ptr %q, i32 218
  store volatile i32 6, ptr %f218
  %f219 = getelementptr i32, ptr %q, i32 219
  store volatile i32 7, ptr %f219
  %f220 = getelementptr i32, ptr %q, i32 220
  store volatile i32 3, ptr %f220
  %f221 = getelementptr i32, ptr %q, i32 221
  store volatile i32 4, ptr %f221
  %f222 = getelementptr i32, ptr %q, i32 222
  store volatile i32 5, ptr %f222
  %f223 = getelementptr i32, ptr %q, i32 223
  store volatile i32 6, ptr %f223
  %f224 = getelementptr i32, ptr %q, i32 224
  store volatile i32 7, ptr %f224
  %f225 = getelementptr i32, ptr %q, i32 225
  store volatile i32 3, ptr %f225
  %f226 = getelementptr i32, ptr %q, i32 226
  store volatile i32 4, ptr %f226
  %f227 = getelementptr i32, ptr %q, i32 227
  store volatile i32 5, ptr %f227
  %f228 = getelementptr i32, ptr %q, i32 228
  store volatile i32 6, ptr %f228
  %f229 = getelementptr i32, ptr %q, i32 229
  store volatile i32 7, ptr %f229
  %f230 = getelementptr i32, ptr %q, i32 230
  store volatile i32 3, ptr %f230
  %f231 = getelementptr i32, ptr %q, i32 231
  store volatile i32 4, ptr %f231
  %f232 = getelementptr i32, ptr %q, i32 232
  store volatile i32 5, ptr %f232
  %f233 = getelementptr i32, ptr %q, i32 233
  store volatile i32 6, ptr %f233
  %f234 = getelementptr i32, ptr %q, i32 234
  store volatile i32 7, ptr %f234
  %f235 = getelementptr i32, ptr %q, i32 235
  store volatile i32 3, ptr %f235
  %f236 = getelementptr i32, ptr %q, i32 236
  store volatile i32 4, ptr %f236
  %f237 = getelementptr i32, ptr %q, i32 237
  store volatile i32 5, ptr %f237
  %f238 = getelementptr i32, ptr %q, i32 238
  store volatile i32 6, ptr %f238
  %f239 = getelementptr i32, ptr %q, i32 239
  store volatile i32 7, ptr %f239
  %f240 = getelementptr i32, ptr %q, i32 240
  store volatile i32 3, ptr %f240
  %f241 = getelementptr i32, ptr %q, i32 241
  store volatile i32 4, ptr %f241
  %f242 = getelementptr i32, ptr %q, i32 242
  store volatile i32 5, ptr %f242
  %f243 = getelementptr i32, ptr %q, i32 243
  store volatile i32 6, ptr %f243
  %f244 = getelementptr i32, ptr %q, i32 244
  store volatile i32 7, ptr %f244
  %f245 = getelementptr i32, ptr %q, i32 245
  store volatile i32 3, ptr %f245
  %f246 = getelementptr i32, ptr %q, i32 246
  store volatile i32 4, ptr %f246
  %f247 = getelementptr i32, ptr %q, i32 247
  store volatile i32 5, ptr %f247
  %f248 = getelementptr i32, ptr %q, i32 248
  store volatile i32 6, ptr %f248
  %f249 = getelementptr i32, ptr %q, i32 249
  store volatile i32 7, ptr %f249
  %f250 = getelementptr i32, ptr %q, i32 250
  store volatile i32 3, ptr %f250
  %f251 = getelementptr i32, ptr %q, i32 251
  store volatile i32 4, ptr %f251
  %f252 = getelementptr i32, ptr %q, i32 252
  store volatile i32 5, ptr %f252
  %f253 = getelementptr i32, ptr %q, i32 253
  store volatile i32 6, ptr %f253
  %f254 = getelementptr i32, ptr %q, i32 254
  store volatile i32 7, ptr %f254
  %f255 = getelementptr i32, ptr %q, i32 255
  store volatile i32 3, ptr %f255
  %f256 = getelementptr i32, ptr %q, i32 256
  store volatile i32 4, ptr %f256
  %f257 = getelementptr i32, ptr %q, i32 257
  store volatile i32 5, ptr %f257
  %f258 = getelementptr i32, ptr %q, i32 258
  store volatile i32 6, ptr %f258
  %f259 = getelementptr i32, ptr %q, i32 259
  store volatile i32 7, ptr %f259
  %f260 = getelementptr i32, ptr %q, i32 260
  store volatile i32 3, ptr %f260
  %f261 = getelementptr i32, ptr %q, i32 261
  store volatile i32 4, ptr %f261
  %f262 = getelementptr i32, ptr %q, i32 262
  store volatile i32 5, ptr %f262
  %f263 = getelementptr i32, ptr %q, i32 263
  store volatile i32 6, ptr %f263
  %f264 = getelementptr i32, ptr %q, i32 264
  store volatile i32 7, ptr %f264
  %f265 = getelementptr i32, ptr %q, i32 265
  store volatile i32 3, ptr %f265
  %f266 = getelementptr i32, ptr %q, i32 266
  store volatile i32 4, ptr %f266
  %f267 = getelementptr i32, ptr %q, i32 267
  store volatile i32 5, ptr %f267
  %f268 = getelementptr i32, ptr %q, i32 268
  store volatile i32 6, ptr %f268
  %f269 = getelementptr i32, ptr %q, i32 269
  store volatile i32 7, ptr %f269
  %f270 = getelementptr i32, ptr %q, i32 270
  store volatile i32 3, ptr %f270
  %f271 = getelementptr i32, ptr %q, i32 271
  store volatile i32 4, ptr %f271
  %f272 = getelementptr i32, ptr %q, i32 272
  store volatile i32 5, ptr %f272
  %f273 = getelementptr i32, ptr %q, i32 273
  store volatile i32 6, ptr %f273
  %f274 = getelementptr i32, ptr %q, i32 274
  store volatile i32 7, ptr %f274
  %f275 = getelementptr i32, ptr %q, i32 275
  store volatile i32 3, ptr %f275
  %f276 = getelementptr i32, ptr %q, i32 276
  store volatile i32 4, ptr %f276
  %f277 = getelementptr i32, ptr %q, i32 277
  store volatile i32 5, ptr %f277
  %f278 = getelementptr i32, ptr %q, i32 278
  store volatile i32 6, ptr %f278
  %f279 = getelementptr i32, ptr %q, i32 279
  store volatile i32 7, ptr %f279
  %f280 = getelementptr i32, ptr %q, i32 280
  store volatile i32 3, ptr %f280
  %f281 = getelementptr i32, ptr %q, i32 281
  store volatile i32 4, ptr %f281
  %f282 = getelementptr i32, ptr %q, i32 282
  store volatile i32 5, ptr %f282
  %f283 = getelementptr i32, ptr %q, i32 283
  store volatile i32 6, ptr %f283
  %f284 = getelementptr i32, ptr %q, i32 284
  store volatile i32 7, ptr %f284
  %f285 = getelementptr i32, ptr %q, i32 285
  store volatile i32 3, ptr %f285
  %f286 = getelementptr i32, ptr %q, i32 286
  store volatile i32 4, ptr %f286
  %f287 = getelementptr i32, ptr %q, i32 287
  store volatile i32 5, ptr %f287
  %f288 = getelementptr i32, ptr %q, i32 288
  store volatile i32 6, ptr %f288
  %f289 = getelementptr i32, ptr %q, i32 289
  store volatile i32 7, ptr %f289
  %f290 = getelementptr i32, ptr %q, i32 290
  store volatile i32 3, ptr %f290
  %f291 = getelementptr i32, ptr %q, i32 291
  store volatile i32 4, ptr %f291
  %f292 = getelementptr i32, ptr %q, i32 292
  store volatile i32 5, ptr %f292
  %f293 = getelementptr i32, ptr %q, i32 293
  store volatile i32 6, ptr %f293
  %f294 = getelementptr i32, ptr %q, i32 294
  store volatile i32 7, ptr %f294
  %f295 = getelementptr i32, ptr %q, i32 295
  store volatile i32 3, ptr %f295
  %f296 = getelementptr i32, ptr %q, i32 296
  store volatile i32 4, ptr %f296
  %f297 = getelementptr i32, ptr %q, i32 297
  store volatile i32 5, ptr %f297
  %f298 = getelementptr i32, ptr %q, i32 298
  store volatile i32 6, ptr %f298
  %f299 = getelementptr i32, ptr %q, i32 299
  store volatile i32 7, ptr %f299
  %f300 = getelementptr i32, ptr %q, i32 300
  store volatile i32 3, ptr %f300
  %f301 = getelementptr i32, ptr %q, i32 301
  store volatile i32 4, ptr %f301
  %f302 = getelementptr i32, ptr %q, i32 302
  store volatile i32 5, ptr %f302
  %f303 = getelementptr i32, ptr %q, i32 303
  store volatile i32 6, ptr %f303
  %f304 = getelementptr i32, ptr %q, i32 304
  store volatile i32 7, ptr %f304
  %f305 = getelementptr i32, ptr %q, i32 305
  store volatile i32 3, ptr %f305
  %f306 = getelementptr i32, ptr %q, i32 306
  store volatile i32 4, ptr %f306
  %f307 = getelementptr i32, ptr %q, i32 307
  store volatile i32 5, ptr %f307
  %f308 = getelementptr i32, ptr %q, i32 308
  store volatile i32 6, ptr %f308
  %f309 = getelementptr i32, ptr %q, i32 309
  store volatile i32 7, ptr %f309
  %f310 = getelementptr i32, ptr %q, i32 310
  store volatile i32 3, ptr %f310
  %f311 = getelementptr i32, ptr %q, i32 311
  store volatile i32 4, ptr %f311
  %f312 = getelementptr i32, ptr %q, i32 312
  store volatile i32 5, ptr %f312
  %f313 = getelementptr i32, ptr %q, i32 313
  store volatile i32 6, ptr %f313
  %f314 = getelementptr i32, ptr %q, i32 314
  store volatile i32 7, ptr %f314
  %f315 = getelementptr i32, ptr %q, i32 315
  store volatile i32 3, ptr %f315
  %f316 = getelementptr i32, ptr %q, i32 316
  store volatile i32 4, ptr %f316
  %f317 = getelementptr i32, ptr %q, i32 317
  store volatile i32 5, ptr %f317
  %f318 = getelementptr i32, ptr %q, i32 318
  store volatile i32 6, ptr %f318
  %f319 = getelementptr i32, ptr %q, i32 319
  store volatile i32 7, ptr %f319
  %f320 = getelementptr i32, ptr %q, i32 320
  store volatile i32 3, ptr %f320
  %f321 = getelementptr i32, ptr %q, i32 321
  store volatile i32 4, ptr %f321
  %f322 = getelementptr i32, ptr %q, i32 322
  store volatile i32 5, ptr %f322
  %f323 = getelementptr i32, ptr %q, i32 323
  store volatile i32 6, ptr %f323
  %f324 = getelementptr i32, ptr %q, i32 324
  store volatile i32 7, ptr %f324
  %f325 = getelementptr i32, ptr %q, i32 325
  store volatile i32 3, ptr %f325
  %f326 = getelementptr i32, ptr %q, i32 326
  store volatile i32 4, ptr %f326
  %f327 = getelementptr i32, ptr %q, i32 327
  store volatile i32 5, ptr %f327
  %f328 = getelementptr i32, ptr %q, i32 328
  store volatile i32 6, ptr %f328
  %f329 = getelementptr i32, ptr %q, i32 329
  store volatile i32 7, ptr %f329
  %f330 = getelementptr i32, ptr %q, i32 330
  store volatile i32 3, ptr %f330
  %f331 = getelementptr i32, ptr %q, i32 331
  store volatile i32 4, ptr %f331
  %f332 = getelementptr i32, ptr %q, i32 332
  store volatile i32 5, ptr %f332
  %f333 = getelementptr i32, ptr %q, i32 333
  store volatile i32 6, ptr %f333
  %f334 = getelementptr i32, ptr %q, i32 334
  store volatile i32 7, ptr %f334
  %f335 = getelementptr i32, ptr %q, i32 335
  store volatile i32 3, ptr %f335
  %f336 = getelementptr i32, ptr %q, i32 336
  store volatile i32 4, ptr %f336
  %f337 = getelementptr i32, ptr %q, i32 337
  store volatile i32 5, ptr %f337
  %f338 = getelementptr i32, ptr %q, i32 338
  store volatile i32 6, ptr %f338
  %f339 = getelementptr i32, ptr %q, i32 339
  store volatile i32 7, ptr %f339
  %f340 = getelementptr i32, ptr %q, i32 340
  store volatile i32 3, ptr %f340
  %f341 = getelementptr i32, ptr %q, i32 341
  store volatile i32 4, ptr %f341
  %f342 = getelementptr i32, ptr %q, i32 342
  store volatile i32 5, ptr %f342
  %f343 = getelementptr i32, ptr %q, i32 343
  store volatile i32 6, ptr %f343
  %f344 = getelementptr i32, ptr %q, i32 344
  store volatile i32 7, ptr %f344
  %f345 = getelementptr i32, ptr %q, i32 345
  store volatile i32 3, ptr %f345
  %f346 = getelementptr i32, ptr %q, i32 346
  store volatile i32 4, ptr %f346
  %f347 = getelementptr i32, ptr %q, i32 347
  store volatile i32 5, ptr %f347
  %f348 = getelementptr i32, ptr %q, i32 348
  store volatile i32 6, ptr %f348
  %f349 = getelementptr i32, ptr %q, i32 349
  store volatile i32 7, ptr %f349
  %f350 = getelementptr i32, ptr %q, i32 350
  store volatile i32 3, ptr %f350
  %f351 = getelementptr i32, ptr %q, i32 351
  store volatile i32 4, ptr %f351
  %f352 = getelementptr i32, ptr %q, i32 352
  store volatile i32 5, ptr %f352
  %f353 = getelementptr i32, ptr %q, i32 353
  store volatile i32 6, ptr %f353
  %f354 = getelementptr i32, ptr %q, i32 354
  store volatile i32 7, ptr %f354
  %f355 = getelementptr i32, ptr %q, i32 355
  store volatile i32 3, ptr %f355
  %f356 = getelementptr i32, ptr %q, i32 356
  store volatile i32 4, ptr %f356
  %f357 = getelementptr i32, ptr %q, i32 357
  store volatile i32 5, ptr %f357
  %f358 = getelementptr i32, ptr %q, i32 358
  store volatile i32 6, ptr %f358
  %f359 = getelementptr i32, ptr %q, i32 359
  store volatile i32 7, ptr %f359
  %f360 = getelementptr i32, ptr %q, i32 360
  store volatile i32 3, ptr %f360
  %f361 = getelementptr i32, ptr %q, i32 361
  store volatile i32 4, ptr %f361
  %f362 = getelementptr i32, ptr %q, i32 362
  store volatile i32 5, ptr %f362
  %f363 = getelementptr i32, ptr %q, i32 363
  store volatile i32 6, ptr %f363
  %f364 = getelementptr i32, ptr %q, i32 364
  store volatile i32 7, ptr %f364
  %f365 = getelementptr i32, ptr %q, i32 365
  store volatile i32 3, ptr %f365
  %f366 = getelementptr i32, ptr %q, i32 366
  store volatile i32 4, ptr %f366
  %f367 = getelementptr i32, ptr %q, i32 367
  store volatile i32 5, ptr %f367
  %f368 = getelementptr i32, ptr %q, i32 368
  store volatile i32 6, ptr %f368
  %f369 = getelementptr i32, ptr %q, i32 369
  store volatile i32 7, ptr %f369
  %f370 = getelementptr i32, ptr %q, i32 370
  store volatile i32 3, ptr %f370
  %f371 = getelementptr i32, ptr %q, i32 371
  store volatile i32 4, ptr %f371
  %f372 = getelementptr i32, ptr %q, i32 372
  store volatile i32 5, ptr %f372
  %f373 = getelementptr i32, ptr %q, i32 373
  store volatile i32 6, ptr %f373
  %f374 = getelementptr i32, ptr %q, i32 374
  store volatile i32 7, ptr %f374
  %f375 = getelementptr i32, ptr %q, i32 375
  store volatile i32 3, ptr %f375
  %f376 = getelementptr i32, ptr %q, i32 376
  store volatile i32 4, ptr %f376
  %f377 = getelementptr i32, ptr %q, i32 377
  store volatile i32 5, ptr %f377
  %f378 = getelementptr i32, ptr %q, i32 378
  store volatile i32 6, ptr %f378
  %f379 = getelementptr i32, ptr %q, i32 379
  store volatile i32 7, ptr %f379
  %f380 = getelementptr i32, ptr %q, i32 380
  store volatile i32 3, ptr %f380
  %f381 = getelementptr i32, ptr %q, i32 381
  store volatile i32 4, ptr %f381
  %f382 = getelementptr i32, ptr %q, i32 382
  store volatile i32 5, ptr %f382
  %f383 = getelementptr i32, ptr %q, i32 383
  store volatile i32 6, ptr %f383
  %f384 = getelementptr i32, ptr %q, i32 384
  store volatile i32 7, ptr %f384
  %f385 = getelementptr i32, ptr %q, i32 385
  store volatile i32 3, ptr %f385
  %f386 = getelementptr i32, ptr %q, i32 386
  store volatile i32 4, ptr %f386
  %f387 = getelementptr i32, ptr %q, i32 387
  store volatile i32 5, ptr %f387
  %f388 = getelementptr i32, ptr %q, i32 388
  store volatile i32 6, ptr %f388
  %f389 = getelementptr i32, ptr %q, i32 389
  store volatile i32 7, ptr %f389
  %f390 = getelementptr i32, ptr %q, i32 390
  store volatile i32 3, ptr %f390
  %f391 = getelementptr i32, ptr %q, i32 391
  store volatile i32 4, ptr %f391
  %f392 = getelementptr i32, ptr %q, i32 392
  store volatile i32 5, ptr %f392
  %f393 = getelementptr i32, ptr %q, i32 393
  store volatile i32 6, ptr %f393
  %f394 = getelementptr i32, ptr %q, i32 394
  store volatile i32 7, ptr %f394
  %f395 = getelementptr i32, ptr %q, i32 395
  store volatile i32 3, ptr %f395
  %f396 = getelementptr i32, ptr %q, i32 396
  store volatile i32 4, ptr %f396
  %f397 = getelementptr i32, ptr %q, i32 397
  store volatile i32 5, ptr %f397
  %f398 = getelementptr i32, ptr %q, i32 398
  store volatile i32 6, ptr %f398
  %f399 = getelementptr i32, ptr %q, i32 399
  store volatile i32 7, ptr %f399
  %f400 = getelementptr i32, ptr %q, i32 400
  store volatile i32 3, ptr %f400
  %f401 = getelementptr i32, ptr %q, i32 401
  store volatile i32 4, ptr %f401
  %f402 = getelementptr i32, ptr %q, i32 402
  store volatile i32 5, ptr %f402
  %f403 = getelementptr i32, ptr %q, i32 403
  store volatile i32 6, ptr %f403
  %f404 = getelementptr i32, ptr %q, i32 404
  store volatile i32 7, ptr %f404
  %f405 = getelementptr i32, ptr %q, i32 405
  store volatile i32 3, ptr %f405
  %f406 = getelementptr i32, ptr %q, i32 406
  store volatile i32 4, ptr %f406
  %f407 = getelementptr i32, ptr %q, i32 407
  store volatile i32 5, ptr %f407
  %f408 = getelementptr i32, ptr %q, i32 408
  store volatile i32 6, ptr %f408
  %f409 = getelementptr i32, ptr %q, i32 409
  store volatile i32 7, ptr %f409
  %f410 = getelementptr i32, ptr %q, i32 410
  store volatile i32 3, ptr %f410
  %f411 = getelementptr i32, ptr %q, i32 411
  store volatile i32 4, ptr %f411
  %f412 = getelementptr i32, ptr %q, i32 412
  store volatile i32 5, ptr %f412
  %f413 = getelementptr i32, ptr %q, i32 413
  store volatile i32 6, ptr %f413
  %f414 = getelementptr i32, ptr %q, i32 414
  store volatile i32 7, ptr %f414
  %f415 = getelementptr i32, ptr %q, i32 415
  store volatile i32 3, ptr %f415
  %f416 = getelementptr i32, ptr %q, i32 416
  store volatile i32 4, ptr %f416
  %f417 = getelementptr i32, ptr %q, i32 417
  store volatile i32 5, ptr %f417
  %f418 = getelementptr i32, ptr %q, i32 418
  store volatile i32 6, ptr %f418
  %f419 = getelementptr i32, ptr %q, i32 419
  store volatile i32 7, ptr %f419
  %f420 = getelementptr i32, ptr %q, i32 420
  store volatile i32 3, ptr %f420
  %f421 = getelementptr i32, ptr %q, i32 421
  store volatile i32 4, ptr %f421
  %f422 = getelementptr i32, ptr %q, i32 422
  store volatile i32 5, ptr %f422
  %f423 = getelementptr i32, ptr %q, i32 423
  store volatile i32 6, ptr %f423
  %f424 = getelementptr i32, ptr %q, i32 424
  store volatile i32 7, ptr %f424
  %f425 = getelementptr i32, ptr %q, i32 425
  store volatile i32 3, ptr %f425
  %f426 = getelementptr i32, ptr %q, i32 426
  store volatile i32 4, ptr %f426
  %f427 = getelementptr i32, ptr %q, i32 427
  store volatile i32 5, ptr %f427
  %f428 = getelementptr i32, ptr %q, i32 428
  store volatile i32 6, ptr %f428
  %f429 = getelementptr i32, ptr %q, i32 429
  store volatile i32 7, ptr %f429
  %f430 = getelementptr i32, ptr %q, i32 430
  store volatile i32 3, ptr %f430
  %f431 = getelementptr i32, ptr %q, i32 431
  store volatile i32 4, ptr %f431
  %f432 = getelementptr i32, ptr %q, i32 432
  store volatile i32 5, ptr %f432
  %f433 = getelementptr i32, ptr %q, i32 433
  store volatile i32 6, ptr %f433
  %f434 = getelementptr i32, ptr %q, i32 434
  store volatile i32 7, ptr %f434
  %f435 = getelementptr i32, ptr %q, i32 435
  store volatile i32 3, ptr %f435
  %f436 = getelementptr i32, ptr %q, i32 436
  store volatile i32 4, ptr %f436
  %f437 = getelementptr i32, ptr %q, i32 437
  store volatile i32 5, ptr %f437
  %f438 = getelementptr i32, ptr %q, i32 438
  store volatile i32 6, ptr %f438
  %f439 = getelementptr i32, ptr %q, i32 439
  store volatile i32 7, ptr %f439
  %f440 = getelementptr i32, ptr %q, i32 440
  store volatile i32 3, ptr %f440
  %f441 = getelementptr i32, ptr %q, i32 441
  store volatile i32 4, ptr %f441
  %f442 = getelementptr i32, ptr %q, i32 442
  store volatile i32 5, ptr %f442
  %f443 = getelementptr i32, ptr %q, i32 443
  store volatile i32 6, ptr %f443
  %f444 = getelementptr i32, ptr %q, i32 444
  store volatile i32 7, ptr %f444
  %f445 = getelementptr i32, ptr %q, i32 445
  store volatile i32 3, ptr %f445
  %f446 = getelementptr i32, ptr %q, i32 446
  store volatile i32 4, ptr %f446
  %f447 = getelementptr i32, ptr %q, i32 447
  store volatile i32 5, ptr %f447
  %f448 = getelementptr i32, ptr %q, i32 448
  store volatile i32 6, ptr %f448
  %f449 = getelementptr i32, ptr %q, i32 449
  store volatile i32 7, ptr %f449
  %f450 = getelementptr i32, ptr %q, i32 450
  store volatile i32 3, ptr %f450
  %f451 = getelementptr i32, ptr %q, i32 451
  store volatile i32 4, ptr %f451
  %f452 = getelementptr i32, ptr %q, i32 452
  store volatile i32 5, ptr %f452
  %f453 = getelementptr i32, ptr %q, i32 453
  store volatile i32 6, ptr %f453
  %f454 = getelementptr i32, ptr %q, i32 454
  store volatile i32 7, ptr %f454
  %f455 = getelementptr i32, ptr %q, i32 455
  store volatile i32 3, ptr %f455
  %f456 = getelementptr i32, ptr %q, i32 456
  store volatile i32 4, ptr %f456
  %f457 = getelementptr i32, ptr %q, i32 457
  store volatile i32 5, ptr %f457
  %f458 = getelementptr i32, ptr %q, i32 458
  store volatile i32 6, ptr %f458
  %f459 = getelementptr i32, ptr %q, i32 459
  store volatile i32 7, ptr %f459
  %f460 = getelementptr i32, ptr %q, i32 460
  store volatile i32 3, ptr %f460
  %f461 = getelementptr i32, ptr %q, i32 461
  store volatile i32 4, ptr %f461
  %f462 = getelementptr i32, ptr %q, i32 462
  store volatile i32 5, ptr %f462
  %f463 = getelementptr i32, ptr %q, i32 463
  store volatile i32 6, ptr %f463
  %f464 = getelementptr i32, ptr %q, i32 464
  store volatile i32 7, ptr %f464
  %f465 = getelementptr i32, ptr %q, i32 465
  store volatile i32 3, ptr %f465
  %f466 = getelementptr i32, ptr %q, i32 466
  store volatile i32 4, ptr %f466
  %f467 = getelementptr i32, ptr %q, i32 467
  store volatile i32 5, ptr %f467
  %f468 = getelementptr i32, ptr %q, i32 468
  store volatile i32 6, ptr %f468
  %f469 = getelementptr i32, ptr %q, i32 469
  store volatile i32 7, ptr %f469
  %f470 = getelementptr i32, ptr %q, i32 470
  store volatile i32 3, ptr %f470
  %f471 = getelementptr i32, ptr %q, i32 471
  store volatile i32 4, ptr %f471
  %f472 = getelementptr i32, ptr %q, i32 472
  store volatile i32 5, ptr %f472
  %f473 = getelementptr i32, ptr %q, i32 473
  store volatile i32 6, ptr %f473
  %f474 = getelementptr i32, ptr %q, i32 474
  store volatile i32 7, ptr %f474
  %f475 = getelementptr i32, ptr %q, i32 475
  store volatile i32 3, ptr %f475
  %f476 = getelementptr i32, ptr %q, i32 476
  store volatile i32 4, ptr %f476
  %f477 = getelementptr i32, ptr %q, i32 477
  store volatile i32 5, ptr %f477
  %f478 = getelementptr i32, ptr %q, i32 478
  store volatile i32 6, ptr %f478
  %f479 = getelementptr i32, ptr %q, i32 479
  store volatile i32 7, ptr %f479
  %f480 = getelementptr i32, ptr %q, i32 480
  store volatile i32 3, ptr %f480
  %f481 = getelementptr i32, ptr %q, i32 481
  store volatile i32 4, ptr %f481
  %f482 = getelementptr i32, ptr %q, i32 482
  store volatile i32 5, ptr %f482
  %f483 = getelementptr i32, ptr %q, i32 483
  store volatile i32 6, ptr %f483
  %f484 = getelementptr i32, ptr %q, i32 484
  store volatile i32 7, ptr %f484
  %f485 = getelementptr i32, ptr %q, i32 485
  store volatile i32 3, ptr %f485
  %f486 = getelementptr i32, ptr %q, i32 486
  store volatile i32 4, ptr %f486
  %f487 = getelementptr i32, ptr %q, i32 487
  store volatile i32 5, ptr %f487
  %f488 = getelementptr i32, ptr %q, i32 488
  store volatile i32 6, ptr %f488
  %f489 = getelementptr i32, ptr %q, i32 489
  store volatile i32 7, ptr %f489
  %f490 = getelementptr i32, ptr %q, i32 490
  store volatile i32 3, ptr %f490
  %f491 = getelementptr i32, ptr %q, i32 491
  store volatile i32 4, ptr %f491
  %f492 = getelementptr i32, ptr %q, i32 492
  store volatile i32 5, ptr %f492
  %f493 = getelementptr i32, ptr %q, i32 493
  store volatile i32 6, ptr %f493
  %f494 = getelementptr i32, ptr %q, i32 494
  store volatile i32 7, ptr %f494
  %f495 = getelementptr i32, ptr %q, i32 495
  store volatile i32 3, ptr %f495
  %f496 = getelementptr i32, ptr %q, i32 496
  store volatile i32 4, ptr %f496
  %f497 = getelementptr i32, ptr %q, i32 497
  store volatile i32 5, ptr %f497
  %f498 = getelementptr i32, ptr %q, i32 498
  store volatile i32 6, ptr %f498
  %f499 = getelementptr i32, ptr %q, i32 499
  store volatile i32 7, ptr %f499
  %f500 = getelementptr i32, ptr %q, i32 500
  store volatile i32 3, ptr %f500
  %f501 = getelementptr i32, ptr %q, i32 501
  store volatile i32 4, ptr %f501
  %f502 = getelementptr i32, ptr %q, i32 502
  store volatile i32 5, ptr %f502
  %f503 = getelementptr i32, ptr %q, i32 503
  store volatile i32 6, ptr %f503
  %f504 = getelementptr i32, ptr %q, i32 504
  store volatile i32 7, ptr %f504
  %f505 = getelementptr i32, ptr %q, i32 505
  store volatile i32 3, ptr %f505
  %f506 = getelementptr i32, ptr %q, i32 506
  store volatile i32 4, ptr %f506
  %f507 = getelementptr i32, ptr %q, i32 507
  store volatile i32 5, ptr %f507
  %f508 = getelementptr i32, ptr %q, i32 508
  store volatile i32 6, ptr %f508
  %f509 = getelementptr i32, ptr %q, i32 509
  store volatile i32 7, ptr %f509
  %f510 = getelementptr i32, ptr %q, i32 510
  store volatile i32 3, ptr %f510
  %f511 = getelementptr i32, ptr %q, i32 511
  store volatile i32 4, ptr %f511
  %f512 = getelementptr i32, ptr %q, i32 512
  store volatile i32 5, ptr %f512
  %f513 = getelementptr i32, ptr %q, i32 513
  store volatile i32 6, ptr %f513
  %f514 = getelementptr i32, ptr %q, i32 514
  store volatile i32 7, ptr %f514
  %f515 = getelementptr i32, ptr %q, i32 515
  store volatile i32 3, ptr %f515
  %f516 = getelementptr i32, ptr %q, i32 516
  store volatile i32 4, ptr %f516
  %f517 = getelementptr i32, ptr %q, i32 517
  store volatile i32 5, ptr %f517
  %f518 = getelementptr i32, ptr %q, i32 518
  store volatile i32 6, ptr %f518
  %f519 = getelementptr i32, ptr %q, i32 519
  store volatile i32 7, ptr %f519
  %f520 = getelementptr i32, ptr %q, i32 520
  store volatile i32 3, ptr %f520
  %f521 = getelementptr i32, ptr %q, i32 521
  store volatile i32 4, ptr %f521
  %f522 = getelementptr i32, ptr %q, i32 522
  store volatile i32 5, ptr %f522
  %f523 = getelementptr i32, ptr %q, i32 523
  store volatile i32 6, ptr %f523
  %f524 = getelementptr i32, ptr %q, i32 524
  store volatile i32 7, ptr %f524
  %f525 = getelementptr i32, ptr %q, i32 525
  store volatile i32 3, ptr %f525
  %f526 = getelementptr i32, ptr %q, i32 526
  store volatile i32 4, ptr %f526
  %f527 = getelementptr i32, ptr %q, i32 527
  store volatile i32 5, ptr %f527
  %f528 = getelementptr i32, ptr %q, i32 528
  store volatile i32 6, ptr %f528
  %f529 = getelementptr i32, ptr %q, i32 529
  store volatile i32 7, ptr %f529
  %f530 = getelementptr i32, ptr %q, i32 530
  store volatile i32 3, ptr %f530
  %f531 = getelementptr i32, ptr %q, i32 531
  store volatile i32 4, ptr %f531
  %f532 = getelementptr i32, ptr %q, i32 532
  store volatile i32 5, ptr %f532
  %f533 = getelementptr i32, ptr %q, i32 533
  store volatile i32 6, ptr %f533
  %f534 = getelementptr i32, ptr %q, i32 534
  store volatile i32 7, ptr %f534
  %f535 = getelementptr i32, ptr %q, i32 535
  store volatile i32 3, ptr %f535
  %f536 = getelementptr i32, ptr %q, i32 536
  store volatile i32 4, ptr %f536
  %f537 = getelementptr i32, ptr %q, i32 537
  store volatile i32 5, ptr %f537
  %f538 = getelementptr i32, ptr %q, i32 538
  store volatile i32 6, ptr %f538
  %f539 = getelementptr i32, ptr %q, i32 539
  store volatile i32 7, ptr %f539
  %f540 = getelementptr i32, ptr %q, i32 540
  store volatile i32 3, ptr %f540
  %f541 = getelementptr i32, ptr %q, i32 541
  store volatile i32 4, ptr %f541
  %f542 = getelementptr i32, ptr %q, i32 542
  store volatile i32 5, ptr %f542
  %f543 = getelementptr i32, ptr %q, i32 543
  store volatile i32 6, ptr %f543
  %f544 = getelementptr i32, ptr %q, i32 544
  store volatile i32 7, ptr %f544
  %f545 = getelementptr i32, ptr %q, i32 545
  store volatile i32 3, ptr %f545
  %f546 = getelementptr i32, ptr %q, i32 546
  store volatile i32 4, ptr %f546
  %f547 = getelementptr i32, ptr %q, i32 547
  store volatile i32 5, ptr %f547
  %f548 = getelementptr i32, ptr %q, i32 548
  store volatile i32 6, ptr %f548
  %f549 = getelementptr i32, ptr %q, i32 549
  store volatile i32 7, ptr %f549
  %f550 = getelementptr i32, ptr %q, i32 550
  store volatile i32 3, ptr %f550
  %f551 = getelementptr i32, ptr %q, i32 551
  store volatile i32 4, ptr %f551
  %f552 = getelementptr i32, ptr %q, i32 552
  store volatile i32 5, ptr %f552
  %f553 = getelementptr i32, ptr %q, i32 553
  store volatile i32 6, ptr %f553
  %f554 = getelementptr i32, ptr %q, i32 554
  store volatile i32 7, ptr %f554
  %f555 = getelementptr i32, ptr %q, i32 555
  store volatile i32 3, ptr %f555
  %f556 = getelementptr i32, ptr %q, i32 556
  store volatile i32 4, ptr %f556
  %f557 = getelementptr i32, ptr %q, i32 557
  store volatile i32 5, ptr %f557
  %f558 = getelementptr i32, ptr %q, i32 558
  store volatile i32 6, ptr %f558
  %f559 = getelementptr i32, ptr %q, i32 559
  store volatile i32 7, ptr %f559
  %f560 = getelementptr i32, ptr %q, i32 560
  store volatile i32 3, ptr %f560
  %f561 = getelementptr i32, ptr %q, i32 561
  store volatile i32 4, ptr %f561
  %f562 = getelementptr i32, ptr %q, i32 562
  store volatile i32 5, ptr %f562
  %f563 = getelementptr i32, ptr %q, i32 563
  store volatile i32 6, ptr %f563
  %f564 = getelementptr i32, ptr %q, i32 564
  store volatile i32 7, ptr %f564
  %f565 = getelementptr i32, ptr %q, i32 565
  store volatile i32 3, ptr %f565
  %f566 = getelementptr i32, ptr %q, i32 566
  store volatile i32 4, ptr %f566
  %f567 = getelementptr i32, ptr %q, i32 567
  store volatile i32 5, ptr %f567
  %f568 = getelementptr i32, ptr %q, i32 568
  store volatile i32 6, ptr %f568
  %f569 = getelementptr i32, ptr %q, i32 569
  store volatile i32 7, ptr %f569
  %f570 = getelementptr i32, ptr %q, i32 570
  store volatile i32 3, ptr %f570
  %f571 = getelementptr i32, ptr %q, i32 571
  store volatile i32 4, ptr %f571
  %f572 = getelementptr i32, ptr %q, i32 572
  store volatile i32 5, ptr %f572
  %f573 = getelementptr i32, ptr %q, i32 573
  store volatile i32 6, ptr %f573
  %f574 = getelementptr i32, ptr %q, i32 574
  store volatile i32 7, ptr %f574
  %f575 = getelementptr i32, ptr %q, i32 575
  store volatile i32 3, ptr %f575
  %f576 = getelementptr i32, ptr %q, i32 576
  store volatile i32 4, ptr %f576
  %f577 = getelementptr i32, ptr %q, i32 577
  store volatile i32 5, ptr %f577
  %f578 = getelementptr i32, ptr %q, i32 578
  store volatile i32 6, ptr %f578
  %f579 = getelementptr i32, ptr %q, i32 579
  store volatile i32 7, ptr %f579
  %f580 = getelementptr i32, ptr %q, i32 580
  store volatile i32 3, ptr %f580
  %f581 = getelementptr i32, ptr %q, i32 581
  store volatile i32 4, ptr %f581
  %f582 = getelementptr i32, ptr %q, i32 582
  store volatile i32 5, ptr %f582
  %f583 = getelementptr i32, ptr %q, i32 583
  store volatile i32 6, ptr %f583
  %f584 = getelementptr i32, ptr %q, i32 584
  store volatile i32 7, ptr %f584
  %f585 = getelementptr i32, ptr %q, i32 585
  store volatile i32 3, ptr %f585
  %f586 = getelementptr i32, ptr %q, i32 586
  store volatile i32 4, ptr %f586
  %f587 = getelementptr i32, ptr %q, i32 587
  store volatile i32 5, ptr %f587
  %f588 = getelementptr i32, ptr %q, i32 588
  store volatile i32 6, ptr %f588
  %f589 = getelementptr i32, ptr %q, i32 589
  store volatile i32 7, ptr %f589
  %f590 = getelementptr i32, ptr %q, i32 590
  store volatile i32 3, ptr %f590
  %f591 = getelementptr i32, ptr %q, i32 591
  store volatile i32 4, ptr %f591
  %f592 = getelementptr i32, ptr %q, i32 592
  store volatile i32 5, ptr %f592
  %f593 = getelementptr i32, ptr %q, i32 593
  store volatile i32 6, ptr %f593
  %f594 = getelementptr i32, ptr %q, i32 594
  store volatile i32 7, ptr %f594
  %f595 = getelementptr i32, ptr %q, i32 595
  store volatile i32 3, ptr %f595
  %f596 = getelementptr i32, ptr %q, i32 596
  store volatile i32 4, ptr %f596
  %f597 = getelementptr i32, ptr %q, i32 597
  store volatile i32 5, ptr %f597
  %f598 = getelementptr i32, ptr %q, i32 598
  store volatile i32 6, ptr %f598
  %f599 = getelementptr i32, ptr %q, i32 599
  store volatile i32 7, ptr %f599
  br label %loop

loop:
  call void @llvm.ezh.tight.loop(ptr blockaddress(@g, %ex2), i32 999)
  call void asm sideeffect "nop", ""()
  %r = call ptr asm sideeffect "str_post $0, $1, 4", "=r,r,0,~{memory}"(i32 0, ptr %p)
  br label %mid

mid:
  store volatile i32 2004318071, ptr %q
  %r2 = call ptr asm sideeffect "str_post $0, $1, 4", "=r,r,0,~{memory}"(i32 0, ptr %p)
  br label %ex2

ex2:
  %h0 = getelementptr i32, ptr %q, i32 600
  store volatile i32 9, ptr %h0
  %h1 = getelementptr i32, ptr %q, i32 601
  store volatile i32 10, ptr %h1
  %h2 = getelementptr i32, ptr %q, i32 602
  store volatile i32 11, ptr %h2
  %h3 = getelementptr i32, ptr %q, i32 603
  store volatile i32 12, ptr %h3
  %h4 = getelementptr i32, ptr %q, i32 604
  store volatile i32 9, ptr %h4
  %h5 = getelementptr i32, ptr %q, i32 605
  store volatile i32 10, ptr %h5
  %h6 = getelementptr i32, ptr %q, i32 606
  store volatile i32 11, ptr %h6
  %h7 = getelementptr i32, ptr %q, i32 607
  store volatile i32 12, ptr %h7
  %h8 = getelementptr i32, ptr %q, i32 608
  store volatile i32 9, ptr %h8
  %h9 = getelementptr i32, ptr %q, i32 609
  store volatile i32 10, ptr %h9
  %h10 = getelementptr i32, ptr %q, i32 610
  store volatile i32 11, ptr %h10
  %h11 = getelementptr i32, ptr %q, i32 611
  store volatile i32 12, ptr %h11
  %h12 = getelementptr i32, ptr %q, i32 612
  store volatile i32 9, ptr %h12
  %h13 = getelementptr i32, ptr %q, i32 613
  store volatile i32 10, ptr %h13
  %h14 = getelementptr i32, ptr %q, i32 614
  store volatile i32 11, ptr %h14
  %h15 = getelementptr i32, ptr %q, i32 615
  store volatile i32 12, ptr %h15
  %h16 = getelementptr i32, ptr %q, i32 616
  store volatile i32 9, ptr %h16
  %h17 = getelementptr i32, ptr %q, i32 617
  store volatile i32 10, ptr %h17
  %h18 = getelementptr i32, ptr %q, i32 618
  store volatile i32 11, ptr %h18
  %h19 = getelementptr i32, ptr %q, i32 619
  store volatile i32 12, ptr %h19
  %h20 = getelementptr i32, ptr %q, i32 620
  store volatile i32 9, ptr %h20
  %h21 = getelementptr i32, ptr %q, i32 621
  store volatile i32 10, ptr %h21
  %h22 = getelementptr i32, ptr %q, i32 622
  store volatile i32 11, ptr %h22
  %h23 = getelementptr i32, ptr %q, i32 623
  store volatile i32 12, ptr %h23
  %h24 = getelementptr i32, ptr %q, i32 624
  store volatile i32 9, ptr %h24
  %h25 = getelementptr i32, ptr %q, i32 625
  store volatile i32 10, ptr %h25
  %h26 = getelementptr i32, ptr %q, i32 626
  store volatile i32 11, ptr %h26
  %h27 = getelementptr i32, ptr %q, i32 627
  store volatile i32 12, ptr %h27
  %h28 = getelementptr i32, ptr %q, i32 628
  store volatile i32 9, ptr %h28
  %h29 = getelementptr i32, ptr %q, i32 629
  store volatile i32 10, ptr %h29
  %h30 = getelementptr i32, ptr %q, i32 630
  store volatile i32 11, ptr %h30
  %h31 = getelementptr i32, ptr %q, i32 631
  store volatile i32 12, ptr %h31
  %h32 = getelementptr i32, ptr %q, i32 632
  store volatile i32 9, ptr %h32
  %h33 = getelementptr i32, ptr %q, i32 633
  store volatile i32 10, ptr %h33
  %h34 = getelementptr i32, ptr %q, i32 634
  store volatile i32 11, ptr %h34
  %h35 = getelementptr i32, ptr %q, i32 635
  store volatile i32 12, ptr %h35
  %h36 = getelementptr i32, ptr %q, i32 636
  store volatile i32 9, ptr %h36
  %h37 = getelementptr i32, ptr %q, i32 637
  store volatile i32 10, ptr %h37
  %h38 = getelementptr i32, ptr %q, i32 638
  store volatile i32 11, ptr %h38
  %h39 = getelementptr i32, ptr %q, i32 639
  store volatile i32 12, ptr %h39
  %h40 = getelementptr i32, ptr %q, i32 640
  store volatile i32 9, ptr %h40
  %h41 = getelementptr i32, ptr %q, i32 641
  store volatile i32 10, ptr %h41
  %h42 = getelementptr i32, ptr %q, i32 642
  store volatile i32 11, ptr %h42
  %h43 = getelementptr i32, ptr %q, i32 643
  store volatile i32 12, ptr %h43
  %h44 = getelementptr i32, ptr %q, i32 644
  store volatile i32 9, ptr %h44
  %h45 = getelementptr i32, ptr %q, i32 645
  store volatile i32 10, ptr %h45
  %h46 = getelementptr i32, ptr %q, i32 646
  store volatile i32 11, ptr %h46
  %h47 = getelementptr i32, ptr %q, i32 647
  store volatile i32 12, ptr %h47
  %h48 = getelementptr i32, ptr %q, i32 648
  store volatile i32 9, ptr %h48
  %h49 = getelementptr i32, ptr %q, i32 649
  store volatile i32 10, ptr %h49
  %h50 = getelementptr i32, ptr %q, i32 650
  store volatile i32 11, ptr %h50
  %h51 = getelementptr i32, ptr %q, i32 651
  store volatile i32 12, ptr %h51
  %h52 = getelementptr i32, ptr %q, i32 652
  store volatile i32 9, ptr %h52
  %h53 = getelementptr i32, ptr %q, i32 653
  store volatile i32 10, ptr %h53
  %h54 = getelementptr i32, ptr %q, i32 654
  store volatile i32 11, ptr %h54
  %h55 = getelementptr i32, ptr %q, i32 655
  store volatile i32 12, ptr %h55
  %h56 = getelementptr i32, ptr %q, i32 656
  store volatile i32 9, ptr %h56
  %h57 = getelementptr i32, ptr %q, i32 657
  store volatile i32 10, ptr %h57
  %h58 = getelementptr i32, ptr %q, i32 658
  store volatile i32 11, ptr %h58
  %h59 = getelementptr i32, ptr %q, i32 659
  store volatile i32 12, ptr %h59
  %h60 = getelementptr i32, ptr %q, i32 660
  store volatile i32 9, ptr %h60
  %h61 = getelementptr i32, ptr %q, i32 661
  store volatile i32 10, ptr %h61
  %h62 = getelementptr i32, ptr %q, i32 662
  store volatile i32 11, ptr %h62
  %h63 = getelementptr i32, ptr %q, i32 663
  store volatile i32 12, ptr %h63
  %h64 = getelementptr i32, ptr %q, i32 664
  store volatile i32 9, ptr %h64
  %h65 = getelementptr i32, ptr %q, i32 665
  store volatile i32 10, ptr %h65
  %h66 = getelementptr i32, ptr %q, i32 666
  store volatile i32 11, ptr %h66
  %h67 = getelementptr i32, ptr %q, i32 667
  store volatile i32 12, ptr %h67
  %h68 = getelementptr i32, ptr %q, i32 668
  store volatile i32 9, ptr %h68
  %h69 = getelementptr i32, ptr %q, i32 669
  store volatile i32 10, ptr %h69
  %h70 = getelementptr i32, ptr %q, i32 670
  store volatile i32 11, ptr %h70
  %h71 = getelementptr i32, ptr %q, i32 671
  store volatile i32 12, ptr %h71
  %h72 = getelementptr i32, ptr %q, i32 672
  store volatile i32 9, ptr %h72
  %h73 = getelementptr i32, ptr %q, i32 673
  store volatile i32 10, ptr %h73
  %h74 = getelementptr i32, ptr %q, i32 674
  store volatile i32 11, ptr %h74
  %h75 = getelementptr i32, ptr %q, i32 675
  store volatile i32 12, ptr %h75
  %h76 = getelementptr i32, ptr %q, i32 676
  store volatile i32 9, ptr %h76
  %h77 = getelementptr i32, ptr %q, i32 677
  store volatile i32 10, ptr %h77
  %h78 = getelementptr i32, ptr %q, i32 678
  store volatile i32 11, ptr %h78
  %h79 = getelementptr i32, ptr %q, i32 679
  store volatile i32 12, ptr %h79
  %h80 = getelementptr i32, ptr %q, i32 680
  store volatile i32 9, ptr %h80
  %h81 = getelementptr i32, ptr %q, i32 681
  store volatile i32 10, ptr %h81
  %h82 = getelementptr i32, ptr %q, i32 682
  store volatile i32 11, ptr %h82
  %h83 = getelementptr i32, ptr %q, i32 683
  store volatile i32 12, ptr %h83
  %h84 = getelementptr i32, ptr %q, i32 684
  store volatile i32 9, ptr %h84
  %h85 = getelementptr i32, ptr %q, i32 685
  store volatile i32 10, ptr %h85
  %h86 = getelementptr i32, ptr %q, i32 686
  store volatile i32 11, ptr %h86
  %h87 = getelementptr i32, ptr %q, i32 687
  store volatile i32 12, ptr %h87
  %h88 = getelementptr i32, ptr %q, i32 688
  store volatile i32 9, ptr %h88
  %h89 = getelementptr i32, ptr %q, i32 689
  store volatile i32 10, ptr %h89
  %h90 = getelementptr i32, ptr %q, i32 690
  store volatile i32 11, ptr %h90
  %h91 = getelementptr i32, ptr %q, i32 691
  store volatile i32 12, ptr %h91
  %h92 = getelementptr i32, ptr %q, i32 692
  store volatile i32 9, ptr %h92
  %h93 = getelementptr i32, ptr %q, i32 693
  store volatile i32 10, ptr %h93
  %h94 = getelementptr i32, ptr %q, i32 694
  store volatile i32 11, ptr %h94
  %h95 = getelementptr i32, ptr %q, i32 695
  store volatile i32 12, ptr %h95
  %h96 = getelementptr i32, ptr %q, i32 696
  store volatile i32 9, ptr %h96
  %h97 = getelementptr i32, ptr %q, i32 697
  store volatile i32 10, ptr %h97
  %h98 = getelementptr i32, ptr %q, i32 698
  store volatile i32 11, ptr %h98
  %h99 = getelementptr i32, ptr %q, i32 699
  store volatile i32 12, ptr %h99
  %h100 = getelementptr i32, ptr %q, i32 700
  store volatile i32 9, ptr %h100
  %h101 = getelementptr i32, ptr %q, i32 701
  store volatile i32 10, ptr %h101
  %h102 = getelementptr i32, ptr %q, i32 702
  store volatile i32 11, ptr %h102
  %h103 = getelementptr i32, ptr %q, i32 703
  store volatile i32 12, ptr %h103
  %h104 = getelementptr i32, ptr %q, i32 704
  store volatile i32 9, ptr %h104
  %h105 = getelementptr i32, ptr %q, i32 705
  store volatile i32 10, ptr %h105
  %h106 = getelementptr i32, ptr %q, i32 706
  store volatile i32 11, ptr %h106
  %h107 = getelementptr i32, ptr %q, i32 707
  store volatile i32 12, ptr %h107
  %h108 = getelementptr i32, ptr %q, i32 708
  store volatile i32 9, ptr %h108
  %h109 = getelementptr i32, ptr %q, i32 709
  store volatile i32 10, ptr %h109
  %h110 = getelementptr i32, ptr %q, i32 710
  store volatile i32 11, ptr %h110
  %h111 = getelementptr i32, ptr %q, i32 711
  store volatile i32 12, ptr %h111
  %h112 = getelementptr i32, ptr %q, i32 712
  store volatile i32 9, ptr %h112
  %h113 = getelementptr i32, ptr %q, i32 713
  store volatile i32 10, ptr %h113
  %h114 = getelementptr i32, ptr %q, i32 714
  store volatile i32 11, ptr %h114
  %h115 = getelementptr i32, ptr %q, i32 715
  store volatile i32 12, ptr %h115
  %h116 = getelementptr i32, ptr %q, i32 716
  store volatile i32 9, ptr %h116
  %h117 = getelementptr i32, ptr %q, i32 717
  store volatile i32 10, ptr %h117
  %h118 = getelementptr i32, ptr %q, i32 718
  store volatile i32 11, ptr %h118
  %h119 = getelementptr i32, ptr %q, i32 719
  store volatile i32 12, ptr %h119
  %h120 = getelementptr i32, ptr %q, i32 720
  store volatile i32 9, ptr %h120
  %h121 = getelementptr i32, ptr %q, i32 721
  store volatile i32 10, ptr %h121
  %h122 = getelementptr i32, ptr %q, i32 722
  store volatile i32 11, ptr %h122
  %h123 = getelementptr i32, ptr %q, i32 723
  store volatile i32 12, ptr %h123
  %h124 = getelementptr i32, ptr %q, i32 724
  store volatile i32 9, ptr %h124
  %h125 = getelementptr i32, ptr %q, i32 725
  store volatile i32 10, ptr %h125
  %h126 = getelementptr i32, ptr %q, i32 726
  store volatile i32 11, ptr %h126
  %h127 = getelementptr i32, ptr %q, i32 727
  store volatile i32 12, ptr %h127
  %h128 = getelementptr i32, ptr %q, i32 728
  store volatile i32 9, ptr %h128
  %h129 = getelementptr i32, ptr %q, i32 729
  store volatile i32 10, ptr %h129
  %h130 = getelementptr i32, ptr %q, i32 730
  store volatile i32 11, ptr %h130
  %h131 = getelementptr i32, ptr %q, i32 731
  store volatile i32 12, ptr %h131
  %h132 = getelementptr i32, ptr %q, i32 732
  store volatile i32 9, ptr %h132
  %h133 = getelementptr i32, ptr %q, i32 733
  store volatile i32 10, ptr %h133
  %h134 = getelementptr i32, ptr %q, i32 734
  store volatile i32 11, ptr %h134
  %h135 = getelementptr i32, ptr %q, i32 735
  store volatile i32 12, ptr %h135
  %h136 = getelementptr i32, ptr %q, i32 736
  store volatile i32 9, ptr %h136
  %h137 = getelementptr i32, ptr %q, i32 737
  store volatile i32 10, ptr %h137
  %h138 = getelementptr i32, ptr %q, i32 738
  store volatile i32 11, ptr %h138
  %h139 = getelementptr i32, ptr %q, i32 739
  store volatile i32 12, ptr %h139
  %h140 = getelementptr i32, ptr %q, i32 740
  store volatile i32 9, ptr %h140
  %h141 = getelementptr i32, ptr %q, i32 741
  store volatile i32 10, ptr %h141
  %h142 = getelementptr i32, ptr %q, i32 742
  store volatile i32 11, ptr %h142
  %h143 = getelementptr i32, ptr %q, i32 743
  store volatile i32 12, ptr %h143
  %h144 = getelementptr i32, ptr %q, i32 744
  store volatile i32 9, ptr %h144
  %h145 = getelementptr i32, ptr %q, i32 745
  store volatile i32 10, ptr %h145
  %h146 = getelementptr i32, ptr %q, i32 746
  store volatile i32 11, ptr %h146
  %h147 = getelementptr i32, ptr %q, i32 747
  store volatile i32 12, ptr %h147
  %h148 = getelementptr i32, ptr %q, i32 748
  store volatile i32 9, ptr %h148
  %h149 = getelementptr i32, ptr %q, i32 749
  store volatile i32 10, ptr %h149
  %h150 = getelementptr i32, ptr %q, i32 750
  store volatile i32 11, ptr %h150
  %h151 = getelementptr i32, ptr %q, i32 751
  store volatile i32 12, ptr %h151
  %h152 = getelementptr i32, ptr %q, i32 752
  store volatile i32 9, ptr %h152
  %h153 = getelementptr i32, ptr %q, i32 753
  store volatile i32 10, ptr %h153
  %h154 = getelementptr i32, ptr %q, i32 754
  store volatile i32 11, ptr %h154
  %h155 = getelementptr i32, ptr %q, i32 755
  store volatile i32 12, ptr %h155
  %h156 = getelementptr i32, ptr %q, i32 756
  store volatile i32 9, ptr %h156
  %h157 = getelementptr i32, ptr %q, i32 757
  store volatile i32 10, ptr %h157
  %h158 = getelementptr i32, ptr %q, i32 758
  store volatile i32 11, ptr %h158
  %h159 = getelementptr i32, ptr %q, i32 759
  store volatile i32 12, ptr %h159
  %h160 = getelementptr i32, ptr %q, i32 760
  store volatile i32 9, ptr %h160
  %h161 = getelementptr i32, ptr %q, i32 761
  store volatile i32 10, ptr %h161
  %h162 = getelementptr i32, ptr %q, i32 762
  store volatile i32 11, ptr %h162
  %h163 = getelementptr i32, ptr %q, i32 763
  store volatile i32 12, ptr %h163
  %h164 = getelementptr i32, ptr %q, i32 764
  store volatile i32 9, ptr %h164
  %h165 = getelementptr i32, ptr %q, i32 765
  store volatile i32 10, ptr %h165
  %h166 = getelementptr i32, ptr %q, i32 766
  store volatile i32 11, ptr %h166
  %h167 = getelementptr i32, ptr %q, i32 767
  store volatile i32 12, ptr %h167
  %h168 = getelementptr i32, ptr %q, i32 768
  store volatile i32 9, ptr %h168
  %h169 = getelementptr i32, ptr %q, i32 769
  store volatile i32 10, ptr %h169
  %h170 = getelementptr i32, ptr %q, i32 770
  store volatile i32 11, ptr %h170
  %h171 = getelementptr i32, ptr %q, i32 771
  store volatile i32 12, ptr %h171
  %h172 = getelementptr i32, ptr %q, i32 772
  store volatile i32 9, ptr %h172
  %h173 = getelementptr i32, ptr %q, i32 773
  store volatile i32 10, ptr %h173
  %h174 = getelementptr i32, ptr %q, i32 774
  store volatile i32 11, ptr %h174
  %h175 = getelementptr i32, ptr %q, i32 775
  store volatile i32 12, ptr %h175
  %h176 = getelementptr i32, ptr %q, i32 776
  store volatile i32 9, ptr %h176
  %h177 = getelementptr i32, ptr %q, i32 777
  store volatile i32 10, ptr %h177
  %h178 = getelementptr i32, ptr %q, i32 778
  store volatile i32 11, ptr %h178
  %h179 = getelementptr i32, ptr %q, i32 779
  store volatile i32 12, ptr %h179
  %h180 = getelementptr i32, ptr %q, i32 780
  store volatile i32 9, ptr %h180
  %h181 = getelementptr i32, ptr %q, i32 781
  store volatile i32 10, ptr %h181
  %h182 = getelementptr i32, ptr %q, i32 782
  store volatile i32 11, ptr %h182
  %h183 = getelementptr i32, ptr %q, i32 783
  store volatile i32 12, ptr %h183
  %h184 = getelementptr i32, ptr %q, i32 784
  store volatile i32 9, ptr %h184
  %h185 = getelementptr i32, ptr %q, i32 785
  store volatile i32 10, ptr %h185
  %h186 = getelementptr i32, ptr %q, i32 786
  store volatile i32 11, ptr %h186
  %h187 = getelementptr i32, ptr %q, i32 787
  store volatile i32 12, ptr %h187
  %h188 = getelementptr i32, ptr %q, i32 788
  store volatile i32 9, ptr %h188
  %h189 = getelementptr i32, ptr %q, i32 789
  store volatile i32 10, ptr %h189
  %h190 = getelementptr i32, ptr %q, i32 790
  store volatile i32 11, ptr %h190
  %h191 = getelementptr i32, ptr %q, i32 791
  store volatile i32 12, ptr %h191
  %h192 = getelementptr i32, ptr %q, i32 792
  store volatile i32 9, ptr %h192
  %h193 = getelementptr i32, ptr %q, i32 793
  store volatile i32 10, ptr %h193
  %h194 = getelementptr i32, ptr %q, i32 794
  store volatile i32 11, ptr %h194
  %h195 = getelementptr i32, ptr %q, i32 795
  store volatile i32 12, ptr %h195
  %h196 = getelementptr i32, ptr %q, i32 796
  store volatile i32 9, ptr %h196
  %h197 = getelementptr i32, ptr %q, i32 797
  store volatile i32 10, ptr %h197
  %h198 = getelementptr i32, ptr %q, i32 798
  store volatile i32 11, ptr %h198
  %h199 = getelementptr i32, ptr %q, i32 799
  store volatile i32 12, ptr %h199
  %h200 = getelementptr i32, ptr %q, i32 800
  store volatile i32 9, ptr %h200
  %h201 = getelementptr i32, ptr %q, i32 801
  store volatile i32 10, ptr %h201
  %h202 = getelementptr i32, ptr %q, i32 802
  store volatile i32 11, ptr %h202
  %h203 = getelementptr i32, ptr %q, i32 803
  store volatile i32 12, ptr %h203
  %h204 = getelementptr i32, ptr %q, i32 804
  store volatile i32 9, ptr %h204
  %h205 = getelementptr i32, ptr %q, i32 805
  store volatile i32 10, ptr %h205
  %h206 = getelementptr i32, ptr %q, i32 806
  store volatile i32 11, ptr %h206
  %h207 = getelementptr i32, ptr %q, i32 807
  store volatile i32 12, ptr %h207
  %h208 = getelementptr i32, ptr %q, i32 808
  store volatile i32 9, ptr %h208
  %h209 = getelementptr i32, ptr %q, i32 809
  store volatile i32 10, ptr %h209
  %h210 = getelementptr i32, ptr %q, i32 810
  store volatile i32 11, ptr %h210
  %h211 = getelementptr i32, ptr %q, i32 811
  store volatile i32 12, ptr %h211
  %h212 = getelementptr i32, ptr %q, i32 812
  store volatile i32 9, ptr %h212
  %h213 = getelementptr i32, ptr %q, i32 813
  store volatile i32 10, ptr %h213
  %h214 = getelementptr i32, ptr %q, i32 814
  store volatile i32 11, ptr %h214
  %h215 = getelementptr i32, ptr %q, i32 815
  store volatile i32 12, ptr %h215
  %h216 = getelementptr i32, ptr %q, i32 816
  store volatile i32 9, ptr %h216
  %h217 = getelementptr i32, ptr %q, i32 817
  store volatile i32 10, ptr %h217
  %h218 = getelementptr i32, ptr %q, i32 818
  store volatile i32 11, ptr %h218
  %h219 = getelementptr i32, ptr %q, i32 819
  store volatile i32 12, ptr %h219
  %h220 = getelementptr i32, ptr %q, i32 820
  store volatile i32 9, ptr %h220
  %h221 = getelementptr i32, ptr %q, i32 821
  store volatile i32 10, ptr %h221
  %h222 = getelementptr i32, ptr %q, i32 822
  store volatile i32 11, ptr %h222
  %h223 = getelementptr i32, ptr %q, i32 823
  store volatile i32 12, ptr %h223
  %h224 = getelementptr i32, ptr %q, i32 824
  store volatile i32 9, ptr %h224
  %h225 = getelementptr i32, ptr %q, i32 825
  store volatile i32 10, ptr %h225
  %h226 = getelementptr i32, ptr %q, i32 826
  store volatile i32 11, ptr %h226
  %h227 = getelementptr i32, ptr %q, i32 827
  store volatile i32 12, ptr %h227
  %h228 = getelementptr i32, ptr %q, i32 828
  store volatile i32 9, ptr %h228
  %h229 = getelementptr i32, ptr %q, i32 829
  store volatile i32 10, ptr %h229
  %h230 = getelementptr i32, ptr %q, i32 830
  store volatile i32 11, ptr %h230
  %h231 = getelementptr i32, ptr %q, i32 831
  store volatile i32 12, ptr %h231
  %h232 = getelementptr i32, ptr %q, i32 832
  store volatile i32 9, ptr %h232
  %h233 = getelementptr i32, ptr %q, i32 833
  store volatile i32 10, ptr %h233
  %h234 = getelementptr i32, ptr %q, i32 834
  store volatile i32 11, ptr %h234
  %h235 = getelementptr i32, ptr %q, i32 835
  store volatile i32 12, ptr %h235
  %h236 = getelementptr i32, ptr %q, i32 836
  store volatile i32 9, ptr %h236
  %h237 = getelementptr i32, ptr %q, i32 837
  store volatile i32 10, ptr %h237
  %h238 = getelementptr i32, ptr %q, i32 838
  store volatile i32 11, ptr %h238
  %h239 = getelementptr i32, ptr %q, i32 839
  store volatile i32 12, ptr %h239
  %h240 = getelementptr i32, ptr %q, i32 840
  store volatile i32 9, ptr %h240
  %h241 = getelementptr i32, ptr %q, i32 841
  store volatile i32 10, ptr %h241
  %h242 = getelementptr i32, ptr %q, i32 842
  store volatile i32 11, ptr %h242
  %h243 = getelementptr i32, ptr %q, i32 843
  store volatile i32 12, ptr %h243
  %h244 = getelementptr i32, ptr %q, i32 844
  store volatile i32 9, ptr %h244
  %h245 = getelementptr i32, ptr %q, i32 845
  store volatile i32 10, ptr %h245
  %h246 = getelementptr i32, ptr %q, i32 846
  store volatile i32 11, ptr %h246
  %h247 = getelementptr i32, ptr %q, i32 847
  store volatile i32 12, ptr %h247
  %h248 = getelementptr i32, ptr %q, i32 848
  store volatile i32 9, ptr %h248
  %h249 = getelementptr i32, ptr %q, i32 849
  store volatile i32 10, ptr %h249
  %h250 = getelementptr i32, ptr %q, i32 850
  store volatile i32 11, ptr %h250
  %h251 = getelementptr i32, ptr %q, i32 851
  store volatile i32 12, ptr %h251
  %h252 = getelementptr i32, ptr %q, i32 852
  store volatile i32 9, ptr %h252
  %h253 = getelementptr i32, ptr %q, i32 853
  store volatile i32 10, ptr %h253
  %h254 = getelementptr i32, ptr %q, i32 854
  store volatile i32 11, ptr %h254
  %h255 = getelementptr i32, ptr %q, i32 855
  store volatile i32 12, ptr %h255
  %h256 = getelementptr i32, ptr %q, i32 856
  store volatile i32 9, ptr %h256
  %h257 = getelementptr i32, ptr %q, i32 857
  store volatile i32 10, ptr %h257
  %h258 = getelementptr i32, ptr %q, i32 858
  store volatile i32 11, ptr %h258
  %h259 = getelementptr i32, ptr %q, i32 859
  store volatile i32 12, ptr %h259
  %h260 = getelementptr i32, ptr %q, i32 860
  store volatile i32 9, ptr %h260
  %h261 = getelementptr i32, ptr %q, i32 861
  store volatile i32 10, ptr %h261
  %h262 = getelementptr i32, ptr %q, i32 862
  store volatile i32 11, ptr %h262
  %h263 = getelementptr i32, ptr %q, i32 863
  store volatile i32 12, ptr %h263
  %h264 = getelementptr i32, ptr %q, i32 864
  store volatile i32 9, ptr %h264
  %h265 = getelementptr i32, ptr %q, i32 865
  store volatile i32 10, ptr %h265
  %h266 = getelementptr i32, ptr %q, i32 866
  store volatile i32 11, ptr %h266
  %h267 = getelementptr i32, ptr %q, i32 867
  store volatile i32 12, ptr %h267
  %h268 = getelementptr i32, ptr %q, i32 868
  store volatile i32 9, ptr %h268
  %h269 = getelementptr i32, ptr %q, i32 869
  store volatile i32 10, ptr %h269
  %h270 = getelementptr i32, ptr %q, i32 870
  store volatile i32 11, ptr %h270
  %h271 = getelementptr i32, ptr %q, i32 871
  store volatile i32 12, ptr %h271
  %h272 = getelementptr i32, ptr %q, i32 872
  store volatile i32 9, ptr %h272
  %h273 = getelementptr i32, ptr %q, i32 873
  store volatile i32 10, ptr %h273
  %h274 = getelementptr i32, ptr %q, i32 874
  store volatile i32 11, ptr %h274
  %h275 = getelementptr i32, ptr %q, i32 875
  store volatile i32 12, ptr %h275
  %h276 = getelementptr i32, ptr %q, i32 876
  store volatile i32 9, ptr %h276
  %h277 = getelementptr i32, ptr %q, i32 877
  store volatile i32 10, ptr %h277
  %h278 = getelementptr i32, ptr %q, i32 878
  store volatile i32 11, ptr %h278
  %h279 = getelementptr i32, ptr %q, i32 879
  store volatile i32 12, ptr %h279
  %h280 = getelementptr i32, ptr %q, i32 880
  store volatile i32 9, ptr %h280
  %h281 = getelementptr i32, ptr %q, i32 881
  store volatile i32 10, ptr %h281
  %h282 = getelementptr i32, ptr %q, i32 882
  store volatile i32 11, ptr %h282
  %h283 = getelementptr i32, ptr %q, i32 883
  store volatile i32 12, ptr %h283
  %h284 = getelementptr i32, ptr %q, i32 884
  store volatile i32 9, ptr %h284
  %h285 = getelementptr i32, ptr %q, i32 885
  store volatile i32 10, ptr %h285
  %h286 = getelementptr i32, ptr %q, i32 886
  store volatile i32 11, ptr %h286
  %h287 = getelementptr i32, ptr %q, i32 887
  store volatile i32 12, ptr %h287
  %h288 = getelementptr i32, ptr %q, i32 888
  store volatile i32 9, ptr %h288
  %h289 = getelementptr i32, ptr %q, i32 889
  store volatile i32 10, ptr %h289
  %h290 = getelementptr i32, ptr %q, i32 890
  store volatile i32 11, ptr %h290
  %h291 = getelementptr i32, ptr %q, i32 891
  store volatile i32 12, ptr %h291
  %h292 = getelementptr i32, ptr %q, i32 892
  store volatile i32 9, ptr %h292
  %h293 = getelementptr i32, ptr %q, i32 893
  store volatile i32 10, ptr %h293
  %h294 = getelementptr i32, ptr %q, i32 894
  store volatile i32 11, ptr %h294
  %h295 = getelementptr i32, ptr %q, i32 895
  store volatile i32 12, ptr %h295
  %h296 = getelementptr i32, ptr %q, i32 896
  store volatile i32 9, ptr %h296
  %h297 = getelementptr i32, ptr %q, i32 897
  store volatile i32 10, ptr %h297
  %h298 = getelementptr i32, ptr %q, i32 898
  store volatile i32 11, ptr %h298
  %h299 = getelementptr i32, ptr %q, i32 899
  store volatile i32 12, ptr %h299
  ret void
}

declare void @llvm.ezh.tight.loop(ptr, i32)
