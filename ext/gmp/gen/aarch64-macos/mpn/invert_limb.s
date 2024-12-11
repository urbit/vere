















































	.text
	.align	3
	.globl	___gmpn_invert_limb 
	
___gmpn_invert_limb:
	lsr	x2, x0, #54
	adrp	x1, approx_tab@GOTPAGE
	and	x2, x2, #0x1fe
	ldr	x1, [x1, approx_tab@GOTPAGEOFF]
	ldrh	w3, [x1,x2]
	lsr	x4, x0, #24
	add	x4, x4, #1
	ubfiz	x2, x3, #11, #16
	umull	x3, w3, w3
	mul	x3, x3, x4
	sub	x2, x2, #1
	sub	x2, x2, x3, lsr #40
	lsl	x3, x2, #60
	mul	x1, x2, x2
	msub	x1, x1, x4, x3
	lsl	x2, x2, #13
	add	x1, x2, x1, lsr #47
	and	x2, x0, #1
	neg	x3, x2
	and	x3, x3, x1, lsr #1
	add	x2, x2, x0, lsr #1
	msub	x2, x1, x2, x3
	umulh	x2, x2, x1
	lsl	x1, x1, #31
	add	x1, x1, x2, lsr #1
	mul	x3, x1, x0
	umulh	x2, x1, x0
	adds	x4, x3, x0
	adc	x0, x2, x0
	sub	x0, x1, x0
	ret
	

		.section	__TEXT,__const
	.align	1
	
	
approx_tab:
	.hword	2045
	.hword	2037
	.hword	2029
	.hword	2021
	.hword	2013
	.hword	2005
	.hword	1998
	.hword	1990
	.hword	1983
	.hword	1975
	.hword	1968
	.hword	1960
	.hword	1953
	.hword	1946
	.hword	1938
	.hword	1931
	.hword	1924
	.hword	1917
	.hword	1910
	.hword	1903
	.hword	1896
	.hword	1889
	.hword	1883
	.hword	1876
	.hword	1869
	.hword	1863
	.hword	1856
	.hword	1849
	.hword	1843
	.hword	1836
	.hword	1830
	.hword	1824
	.hword	1817
	.hword	1811
	.hword	1805
	.hword	1799
	.hword	1792
	.hword	1786
	.hword	1780
	.hword	1774
	.hword	1768
	.hword	1762
	.hword	1756
	.hword	1750
	.hword	1745
	.hword	1739
	.hword	1733
	.hword	1727
	.hword	1722
	.hword	1716
	.hword	1710
	.hword	1705
	.hword	1699
	.hword	1694
	.hword	1688
	.hword	1683
	.hword	1677
	.hword	1672
	.hword	1667
	.hword	1661
	.hword	1656
	.hword	1651
	.hword	1646
	.hword	1641
	.hword	1636
	.hword	1630
	.hword	1625
	.hword	1620
	.hword	1615
	.hword	1610
	.hword	1605
	.hword	1600
	.hword	1596
	.hword	1591
	.hword	1586
	.hword	1581
	.hword	1576
	.hword	1572
	.hword	1567
	.hword	1562
	.hword	1558
	.hword	1553
	.hword	1548
	.hword	1544
	.hword	1539
	.hword	1535
	.hword	1530
	.hword	1526
	.hword	1521
	.hword	1517
	.hword	1513
	.hword	1508
	.hword	1504
	.hword	1500
	.hword	1495
	.hword	1491
	.hword	1487
	.hword	1483
	.hword	1478
	.hword	1474
	.hword	1470
	.hword	1466
	.hword	1462
	.hword	1458
	.hword	1454
	.hword	1450
	.hword	1446
	.hword	1442
	.hword	1438
	.hword	1434
	.hword	1430
	.hword	1426
	.hword	1422
	.hword	1418
	.hword	1414
	.hword	1411
	.hword	1407
	.hword	1403
	.hword	1399
	.hword	1396
	.hword	1392
	.hword	1388
	.hword	1384
	.hword	1381
	.hword	1377
	.hword	1374
	.hword	1370
	.hword	1366
	.hword	1363
	.hword	1359
	.hword	1356
	.hword	1352
	.hword	1349
	.hword	1345
	.hword	1342
	.hword	1338
	.hword	1335
	.hword	1332
	.hword	1328
	.hword	1325
	.hword	1322
	.hword	1318
	.hword	1315
	.hword	1312
	.hword	1308
	.hword	1305
	.hword	1302
	.hword	1299
	.hword	1295
	.hword	1292
	.hword	1289
	.hword	1286
	.hword	1283
	.hword	1280
	.hword	1276
	.hword	1273
	.hword	1270
	.hword	1267
	.hword	1264
	.hword	1261
	.hword	1258
	.hword	1255
	.hword	1252
	.hword	1249
	.hword	1246
	.hword	1243
	.hword	1240
	.hword	1237
	.hword	1234
	.hword	1231
	.hword	1228
	.hword	1226
	.hword	1223
	.hword	1220
	.hword	1217
	.hword	1214
	.hword	1211
	.hword	1209
	.hword	1206
	.hword	1203
	.hword	1200
	.hword	1197
	.hword	1195
	.hword	1192
	.hword	1189
	.hword	1187
	.hword	1184
	.hword	1181
	.hword	1179
	.hword	1176
	.hword	1173
	.hword	1171
	.hword	1168
	.hword	1165
	.hword	1163
	.hword	1160
	.hword	1158
	.hword	1155
	.hword	1153
	.hword	1150
	.hword	1148
	.hword	1145
	.hword	1143
	.hword	1140
	.hword	1138
	.hword	1135
	.hword	1133
	.hword	1130
	.hword	1128
	.hword	1125
	.hword	1123
	.hword	1121
	.hword	1118
	.hword	1116
	.hword	1113
	.hword	1111
	.hword	1109
	.hword	1106
	.hword	1104
	.hword	1102
	.hword	1099
	.hword	1097
	.hword	1095
	.hword	1092
	.hword	1090
	.hword	1088
	.hword	1086
	.hword	1083
	.hword	1081
	.hword	1079
	.hword	1077
	.hword	1074
	.hword	1072
	.hword	1070
	.hword	1068
	.hword	1066
	.hword	1064
	.hword	1061
	.hword	1059
	.hword	1057
	.hword	1055
	.hword	1053
	.hword	1051
	.hword	1049
	.hword	1047
	.hword	1044
	.hword	1042
	.hword	1040
	.hword	1038
	.hword	1036
	.hword	1034
	.hword	1032
	.hword	1030
	.hword	1028
	.hword	1026
	.hword	1024
