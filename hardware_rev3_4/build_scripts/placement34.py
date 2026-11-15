# Rev 3.4 placement: (ref, anchor pad, x mm, y mm, rotation deg)
# Only parts that are new in Rev 3.4 or that moved from their Rev 3.3 position
# are listed. Everything else stays where the Rev 3.3 board has it.
P = [
    # ---- driver supply switch: Q5 sits in line on the 12 V spine, emitter up
    ('Q5', '1', 108.70, 69.30, 270),
    ('R23', '1', 120.42, 69.90, 180),      # base pull-up to the 12 V rail
    ('R22', '1', 125.12, 73.10, 180),      # base drive, bridges the SW_SENSE line
    ('R26', '1', 115.78, 86.60, 0),        # readback divider top, bridges D2_RPM
    # the INC driver's base resistors move down one row to make room for Q5
    ('R11', '1', 113.12, 77.20, 180), ('R7', '1', 105.50, 80.40, 0),
    ('D4', '1', 102.90, 84.20, 0),         # 1.0 mm west, clears R26
    # the 12 V pull-up on the opto output moves 1.4 mm down, opening two track lanes above its lower pad
    ('R2', '1', 127.90, 56.20, 270),
    # readback divider bottoms move to the Nano end of their lines
    ('R15', '1', 149.40, 91.28, 0), ('R17', '1', 149.40, 83.66, 0), ('R27', '1', 149.40, 94.70, 0),
    ('R20', '1', 160.00, 78.58, 270), ('R21', '1', 164.00, 81.12, 270),      # 2.0 mm east, clears R17
    # ---- switch driver, south-east of the Nano
    ('Q6', '2', 151.20, 111.14, 90),
    ('R24', '1', 154.60, 118.22, 90), ('R25', '1', 158.00, 118.22, 90),
    # ---- optional fault lamp (not fitted by default)
    ('R31', '1', 161.40, 116.20, 90), ('R30', '1', 164.80, 116.20, 90),
    ('Q7', '2', 169.10, 108.58, 90),
    ('R29', '1', 174.10, 103.44, 180),
    ('R28', '1', 173.50, 99.24, 90),       # 15.24 mm pitch: bridges the three lines to J2
    ('J3', '1', 177.03, 100.90, 0),
]

# extra silkscreen legends: (text, x, y, height) -- left/bottom justified like the connector legends
SILK_TEXT = [
    ('+', 179.30, 101.55, 1.3),
    ('FAULT LAMP', 171.40, 107.30, 1.0),
]

# reference designators that need a hand-picked position: (ref, x, y)
REF_AT = [
    ('J1', 92.2, 106.0), ('Q1', 98.6, 76.46), ('Q2', 98.6, 91.66),
    ('Q3', 115.3, 74.2), ('Q6', 150.83, 105.8), ('Q7', 172.0, 110.7),
]
