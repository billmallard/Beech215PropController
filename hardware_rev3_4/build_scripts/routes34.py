# Rev 3.4 routing. Everything is on F.Cu; B.Cu stays an unbroken ground plane.
# R(net, width, point, point, ...)  -- a point is 'REF.PAD' or (x, y) in mm.
PW, DR, VC, SG, HV = 1.0, 0.5, 0.8, 0.4, 0.5   # power, driver, 5 V, signal, magneto widths

# ---- 12 V entry: rail along y = 68.5 to the spine, spine north to the bulk cap and regulator
R('/12V_IN', PW, 'J1.1', 'F1.1')
R('/12V_RAW', PW, 'F1.2', (108.7, 68.5))
R('/12V_RAW', PW, 'D5.1', (98.6, 68.5))
R('/12V_RAW', PW, (108.7, 52.2), 'Q5.1')                                               # spine, under the bridge resistors, ends on the Q5 emitter
R('/12V_RAW', PW, 'C2.1', (108.7, 52.2), (172.13, 52.2), 'PS1.1')                      # bulk cap, run to the regulator
R('/12V_RAW', SG, (108.7, 68.2), (120.42, 68.2), 'R23.1')                              # Q5 base pull-up
R('/12V_RAW', DR, 'PS1.1', (178.3, 54.84), (178.3, 81.5), (175.8, 84.0), 'R28.2')      # fault lamp feed, ahead of the switch

# ---- 12 V switched: from the Q5 collector to both driver cells
R('/12V_SW', PW, 'Q5.3', (108.7, 86.46))                                               # spine continues south under R11, R7, D4
R('/12V_SW', PW, 'Q5.3', (101.96, 74.38), 'Q1.1')                                      # INC driver emitter
R('/12V_SW', DR, (108.7, 82.2), (113.12, 82.2), 'R7.2')                                # INC base pull-up
R('/12V_SW', PW, 'Q2.1', (101.5, 86.46), (113.12, 86.46), 'R8.1')                      # DEC cell rail
R('/12V_SW', DR, (113.12, 86.46), 'R26.1')                                             # switch readback

# ---- Q5 base
R('Net-(Q5-B)', SG, 'Q5.2', (112.8, 71.84), 'R23.2')
R('Net-(Q5-B)', SG, (112.8, 71.84), (116.24, 71.84), 'R22.2')
R('Net-(Q6-C)', SG, 'R22.1', (125.12, 68.7), (134.7, 68.7), (135.8, 69.8), (135.8, 107.6), (136.8, 108.6), 'Q6.3')
R('Net-(Q6-B)', SG, 'Q6.2', 'R24.2')
R('/D5_DRV_EN', SG, 'A1.8', (132.3, 88.74), (133.0, 89.44), (133.0, 112.8), (138.4, 118.2), 'R24.1', 'R25.1')

# ---- INC driver
R('/D6_INC', DR, 'A1.9', (129.2, 91.28), (129.2, 84.48), 'R10.1', 'R5.1')
R('Net-(Q3-B)', DR, 'R5.2', 'Q3.2')
R('/Q3_COLL', DR, 'Q3.3', (113.86, 76.46), 'R11.1')
R('/Q1_BASE', DR, 'Q1.2', (104.76, 76.46), 'R11.2', 'R7.1')
R('/INC_RELAY', PW, 'Q1.3', (98.5, 79.0), (97.0, 80.5), (97.0, 88.77), (95.5, 90.27), 'J1.3')
R('/INC_RELAY', PW, 'Q1.3', (101.5, 82.8), 'D4.1')

# ---- DEC driver
R('/D7_DEC', DR, 'A1.10', 'R6.1', 'R12.1')
R('Net-(Q4-B)', DR, 'R6.2', 'Q4.2')
R('/Q4_COLL', DR, 'Q4.3', 'R13.1')
R('/Q2_BASE', DR, 'Q2.2', 'R13.2', 'R8.2')
R('/DEC_RELAY', PW, 'Q2.3', (99.0, 94.2), (97.61, 92.81), 'J1.4')
R('/DEC_RELAY', PW, 'Q2.3', (101.5, 97.0), 'D3.1')

# ---- analog taps up the left margin, across to the bridge resistors
R('/INC_RELAY', SG, 'J1.3', (88.2, 90.27), (88.2, 67.3), (89.0, 66.5), 'R14.1')
R('/DEC_RELAY', SG, 'J1.4', (87.2, 92.81), (87.2, 64.1), (88.0, 63.3), 'R16.1')
R('/POT_HI', SG, 'J1.5', (86.2, 95.35), (86.2, 57.7), (87.0, 56.9), 'R18.2', 'R19.1')

# ---- sense nodes into the Nano, down the channel between its pin rows
R('/POT_SENSE', SG, 'R19.2', 'C6.1', (141.2, 60.1), (143.2, 62.1), (143.2, 80.12), (144.2, 81.12), 'A1.26')
R('/DEC_SENSE', SG, 'R16.2', (139.4, 63.3), (141.4, 65.3), (141.4, 82.66), (142.4, 83.66), 'A1.25', 'R17.1')
R('/INC_SENSE', SG, 'R14.2', (137.6, 66.5), (139.6, 68.5), (139.6, 90.28), (140.6, 91.28), 'A1.22', 'R15.1')
R('/SW_SENSE', SG, 'R26.2', (123.4, 67.6), (136.2, 67.6), (137.2, 68.6), (137.2, 92.82), (138.2, 93.82), 'A1.21',
  (148.52, 93.82), 'R27.1')

# ---- 5 V
R('/VCC5', VC, 'PS1.3', (172.5, 59.92), (170.49, 57.91), 'C3.1')
R('/VCC5', VC, 'C3.1', (162.28, 54.8), (114.62, 54.8), 'R18.1')                         # pot pull-up
R('/VCC5', VC, (127.9, 54.8), 'R2.1')                                                  # opto pull-up
R('/VCC5', VC, (169.5, 57.91), (169.5, 86.0), (171.57, 88.07), 'J2.1')
R('/VCC5', VC, (169.5, 78.58), (152.2, 78.58), 'C4.1', (148.59, 78.58), 'A1.27')
R('/VCC5', VC, 'R21.1', (164.0, 78.58))
R('/VCC5', VC, 'J2.1', (181.0, 88.07), (181.0, 108.5), (169.3, 120.2), (116.83, 120.2), 'U1.8', 'U1.7', 'C5.1')

# ---- I2C
R('/SCL', SG, 'A1.24', 'R20.2', (166.0, 86.2), (172.95, 93.15), 'J2.3')
R('/SDA', SG, 'A1.23', 'R21.2', (164.5, 88.74), (171.45, 95.69), 'J2.4')

# ---- magneto input
R('/MAG_LEAD', HV, 'J1.7', 'R1.1')
R('/MAG_MID', HV, 'R1.2', 'R9.1')
R('/MAG_FILT', SG, 'C1.1', 'R9.2', 'D6.1')
R('/MAG_FILT', SG, 'R9.2', (114.29, 110.59), 'U1.2')
# opto output: up the west side under R6, R12, R26, R10 and R5 (it no longer uses the Nano channel)
R('/D2_RPM', SG, 'U1.6', (111.75, 113.4), (112.25, 112.9), (119.4, 112.9), (119.9, 112.4), (119.9, 99.4), (122.7, 96.6),
  (124.6, 96.6), (125.2, 96.0), (125.2, 77.6), (125.8, 77.0), (129.3, 77.0))
R('/D2_RPM', SG, 'R2.2', (129.3, 72.84), (129.3, 79.15), 'A1.5')

# ---- indicator LEDs
R('/D8_INC_LED', SG, 'A1.11', (128.0, 96.36), (123.0, 101.36), (121.75, 102.61), 'R3.1')
R('/D9_DEC_LED', SG, 'A1.12', (129.0, 98.9), (126.31, 101.59), 'R4.1')
R('Net-(D1-A)', SG, 'R3.2', 'D1.2')
R('Net-(D2-A)', SG, 'R4.2', 'D2.2')

# ---- optional fault lamp
R('/D4_FAULT', SG, 'A1.7', (133.6, 86.2), (134.4, 87.0), (134.4, 111.4), (139.2, 116.2), 'R31.1', 'R30.1')
R('Net-(Q7-B)', SG, 'R30.2', 'Q7.2')
R('Net-(Q7-C)', SG, 'Q7.3', 'R29.2')
R('/LAMP_N', SG, 'R29.1', 'J3.2')
R('/LAMP_P', SG, 'R28.1', (175.16, 100.9), 'J3.1')
