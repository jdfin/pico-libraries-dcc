import time
import serial as ps

track_tests = [
    # errors detected by dcc_cmd
    [ 'T', 'ERROR' ],           # argc < 2
    [ 'T X', 'ERROR' ],         # argv[1] invalid
    [ 'T 2', 'ERROR' ],         # argv[1] invalid
    [ 'T G 0', 'ERROR' ],       # "T G", argc != 2
    [ 'T S', 'ERROR' ],         # "T S", argc != 3
    [ 'T S X', 'ERROR' ],       # "T S", argv[2] invalid
    # errors detected by dcc_srv
    # (none that can get thru dcc_api)
    # normal operation
    [ 'T S 1', '[Ok]'],         # track on
    [ 'T G', ' on ... [Ok]'],    #
    [ 'T S 0', '[Ok]'],         # track off
    [ 'T G', ' off ... [Ok]'],   #
]

cv_tests = [
    # setup
    [ 'T S 0', '[Ok]' ],            # track off
    # errors detected by dcc_cmd
    [ 'C', 'ERROR' ],               # argc < 2
    [ 'C X', 'ERROR' ],             # argv[1] invalid
    [ 'C 8', 'ERROR' ],             # argc < 3
    [ 'C 8 X', 'ERROR' ],           # argv[2] invalid
    [ 'C 8 G X', 'ERROR' ],         # "C <cv_num> G", argc != 3
    [ 'C 8 S', 'ERROR' ],           # "C <cv_num> S", argc != 4
    [ 'C 8 S 8 X', 'ERROR' ],       # "C <cv_num> S", argc != 4
    [ 'C 8 S X', 'ERROR' ],         # "C <cv_num> S", argv[3] invalid
    [ 'C 8 B', 'ERROR' ],           # "C <cv_num> B", argc < 5
    [ 'C 8 B X', 'ERROR' ],         # "C <cv_num> B", argv[3] invalid
    [ 'C 8 B 0 G X', 'ERROR' ],     # "C <cv_num> B <bit_num> G", argc != 5
    [ 'C 8 B 0 S', 'ERROR' ],       # "C <cv_num> B <bit_num> S", argc != 6
    [ 'C 8 B 0 S 1 X', 'ERROR' ],   # "C <cv_num> B <bit_num> S", argc != 6
    [ 'C 8 B 0 S X', 'ERROR' ],     # "C <cv_num> B <bit_num> S", argv[5] invalid
    # errors detected by dcc_srv
    [ 'C 0 G', '[Error]' ],         # cv_num out of range
    [ 'C 1025 G', '[Error]' ],      # cv_num out of range
    [ 'C 0 S 0', '[Error]' ],       # cv_num out of range
    [ 'C 1025 S 0', '[Error]' ],    # cv_num out of range
    [ 'C 3 S 256', '[Error]' ],     # cv_val out of range
    [ 'C 3 B 8 G', '[Error]' ],     # bit_num out of range
    [ 'C 3 B 8 S 0', '[Error]' ],   # bit_num out of range
    [ 'C 3 B 0 S 2', '[Error]' ],   # bit_val out of range
    [ 'T S 1', '[Ok]' ],            # track on
    [ 'C 3 G', '[Error]' ],         # track must be off
    [ 'C 3 S 0', '[Error]' ],       # track must be off
    [ 'C 3 B 0 S 1', '[Error]' ],   # track must be off
    [ 'T S 0', '[Ok]' ],            # track off
    # normal operation
    [ 'C 8 S 8', '[Ok]' ],          # reset loco to adrs 3
    [ 'C 8 G', ' 0x97 ... [Ok]' ],   # get cv8 (assumes ESU)
    [ 'C 3 G', ' 0x14 ... [Ok]' ],   # get cv3 (acceleration)
    [ 'C 3 S 48', '[Ok]' ],         # set cv3
    [ 'C 3 G', ' 0x30 ... [Ok]' ],   # get cv3
    [ 'C 3 B 5 G', ' 1 ... [Ok]' ],  # get cv3 bit 5
    [ 'C 3 B 5 S 0', '[Ok]' ],      # cv3 bit 5 = 0
    [ 'C 3 B 2 G', ' 0 ... [Ok]' ],  # get cv3 bit 2
    [ 'C 3 B 2 S 1', '[Ok]' ],      # cv3 bit 2 = 1
    [ 'C 3 G', ' 0x14 ... [Ok]' ],   # get cv3
    # cleanup
    [ 'T S 0', '[Ok]' ],            # track off
]

address_tests = [
    # setup
    [ 'T S 0', '[Ok]' ],            # track off
    # errors detected by dcc_cmd
    [ 'A', 'ERROR' ],               # argc < 2
    [ 'A X', 'ERROR' ],             # argv[1] invalid
    [ 'A G X', 'ERROR' ],           # "A G", argc != 2
    [ 'A S 3 X', 'ERROR' ],         # "A S <a>", argc != 3
    [ 'A S X', 'ERROR' ],           # "A S <a>", argv[2] invalid
    # errors detected by dcc_srv
    [ 'A S 0', '[Error]' ],         # "A S <a>", address out of range
    [ 'A S 11000', '[Error]' ],     # "A S <a>", address out of range
    [ 'T S 1', '[Ok]' ],            # track on
    [ 'A G', '[Error]' ],           # track must be off
    [ 'A S 3', '[Error]' ],         # track must be off
    [ 'T S 0', '[Ok]' ],            # track off
    # normal operation
    [ 'C 8 S 8', '[Ok]' ],          # reset loco to adrs 3
    [ 'A G', ' 3 ... [Ok]' ],       # get short address
    [ 'A S 7890', '[Ok]' ],         # set long address
    [ 'A G', ' 7890 ... [Ok]' ],    # get long address
    [ 'A S 3', '[Ok]' ],            # set short address
    [ 'A G', ' 3 ... [Ok]' ],       # get short address
    # cleanup
    [ 'C 8 S 8', '[Ok]' ],          # reset loco to adrs 3
]

loco_tests = [
    # argc < 2
    [ 'L', 'ERROR' ],           # argc < 2
    # argc == 2
    [ 'L X', 'ERROR' ],         # argv[1] not a number
    [ 'L 20000', 'ERROR' ],     # argv[1] out of range
    [ 'L 3', 'OK' ],
    [ 'L ?', '3' ],
    [ 'L 2265', 'OK' ],
    [ 'L ?', '2265' ],
    [ 'L 3', 'OK' ],
    [ 'L ?', '3' ],
    # argc == 3
    [ 'L 3 3', 'ERROR' ],       # argv[1] invalid
    [ 'L + 4', 'OK' ],
    [ 'L - 4', 'OK' ],
    [ 'L - 4', 'OK' ],
    # argc > 3
    [ 'L 3 4 5', 'ERROR' ],     # argc > 3
]

speed_tests = [
    # argc < 2
    [ 'S', 'ERROR' ],           # argc < 2
    # argc == 2
    [ 'S X', 'ERROR' ],         # argv[1] invalid
    [ 'S 200', 'ERROR' ],       # speed out of range
    [ 'S -200', 'ERROR' ],      # speed out of range
    [ 'S 50', 'OK' ],
    [ 'S -50', 'OK' ],
    [ 'S 0', 'OK' ],
    [ 'S ?', '0' ],
    # argc > 2
    [ 'S 3 X', 'ERROR' ],       # argc > 2
]

function_tests = [
    # argc < 2
    [ 'F', 'ERROR' ],           # argc < 2
    # argc == 2
    [ 'F 0', 'ERROR' ],         #
    [ 'F X', 'ERROR' ],         #
    # argc == 3
    [ 'F -1 OFF', 'ERROR' ],    # function number out of range
    [ 'F 0 OFF', 'OK' ],
    [ 'F 1 OFF', 'OK' ],
    [ 'F 2 OFF', 'OK' ],
    [ 'F 3 OFF', 'OK' ],
    [ 'F 4 OFF', 'OK' ],
    [ 'F 5 OFF', 'OK' ],
    [ 'F 6 OFF', 'OK' ],
    [ 'F 7 OFF', 'OK' ],
    [ 'F 8 OFF', 'OK' ],
    [ 'F 9 OFF', 'OK' ],
    [ 'F 10 OFF', 'OK' ],
    [ 'F 11 OFF', 'OK' ],
    [ 'F 12 OFF', 'OK' ],
    [ 'F 13 OFF', 'OK' ],
    [ 'F 14 OFF', 'OK' ],
    [ 'F 15 OFF', 'OK' ],
    [ 'F 16 OFF', 'OK' ],
    [ 'F 17 OFF', 'OK' ],
    [ 'F 18 OFF', 'OK' ],
    [ 'F 19 OFF', 'OK' ],
    [ 'F 20 OFF', 'OK' ],
    [ 'F 21 OFF', 'OK' ],
    [ 'F 22 OFF', 'OK' ],
    [ 'F 23 OFF', 'OK' ],
    [ 'F 24 OFF', 'OK' ],
    [ 'F 25 OFF', 'OK' ],
    [ 'F 26 OFF', 'OK' ],
    [ 'F 27 OFF', 'OK' ],
    [ 'F 28 OFF', 'OK' ],
    [ 'F 29 OFF', 'OK' ],
    [ 'F 30 OFF', 'OK' ],
    [ 'F 31 OFF', 'OK' ],
    [ 'F 32 OFF', 'ERROR' ],    # function number out of range
    [ 'F X ON', 'ERROR' ],      #
    [ 'F ? ON', 'ERROR' ],      #
    [ 'F ? X', 'ERROR' ],       #
    [ 'F 0 ON', 'OK' ],
    [ 'F 0 ?', 'ON' ],
    [ 'F 0 OFF', 'OK' ],
    [ 'F 0 ?', 'OFF' ],
    [ 'F 0 X', 'ERROR' ],       # argv[2] invalid
    # argc > 3
    [ 'F 0 1 ON', 'ERROR' ],    # argc > 3
]

railcom_tests = [
    [ 'C 8 8', 'OK' ],
    [ 'C 31 0', 'OK' ],
    [ 'C 32 255', 'OK' ],
    # manufacturer ID
    [ 'C 257 ?', '' ], # 0
    [ 'C 258 ?', '' ], # 1
    [ 'C 259 ?', '' ], # 2
    [ 'C 260 ?', '' ], # 3
    # product ID
    [ 'C 261 ?', '' ], # 4
    [ 'C 262 ?', '' ], # 5
    [ 'C 263 ?', '' ], # 6
    [ 'C 264 ?', '' ], # 7
    # serial number
    [ 'C 265 ?', '' ], # 8
    [ 'C 266 ?', '' ], # 9
    [ 'C 267 ?', '' ], # 10
    [ 'C 268 ?', '' ], # 11
    # production date
    [ 'C 269 ?', '' ], # 12
    [ 'C 270 ?', '' ], # 13
    [ 'C 271 ?', '' ], # 14
    [ 'C 272 ?', '' ], # 15
]

# MP1130:
# mfg_id:  00_00_00_97 = 151, ok
# prd_id:  02_00_00_93 = 33,554,579 maybe 147
# ser_num: df_fa_b4_ac = 3,757,749,420
# prd_dat: 27_d2_e4_be = 668,132,542 sec = ~21.17 years, ok

# SP2265:
# mfg_id:  00_00_00_97
# prd_id:  02_00_00_9e
# ser_num: f9_e8_67_69
# prd_dat: 2c_64_8d_8a

# UP 852:
# mfg_id:  00_00_00_97 = 151, ok
# prd_id:  02_00_00_db
# ser_num: df_f5_f5_d4
# prd_dat: 2b_2f_d0_0f

tests = [
    track_tests,
    cv_tests,
    address_tests,
    #loco_tests,
    #speed_tests,
    #function_tests,
    #railcom_tests,
]


def flush_in():
    time.sleep(0.1)
    while port.in_waiting > 0:
        x = port.read(port.in_waiting)
        time.sleep(0.1)


def one_test(cmd, exp):
    flush_in()
    correct = True
    port.write((cmd + '\r\n').encode('utf-8'))
    echo = port.readline().decode('utf-8').replace('\r', '').replace('\n', '')
    if echo != cmd:
        correct = False
    rsp = port.readline().decode('utf-8').replace('\r', '').replace('\n', '')
    if correct:
        correct = (exp in rsp)
    global pass_count, fail_count
    if correct:
        pass_count += 1
        print(f"{cmd:14} -->  {echo:14} -->  {rsp:54} >>>>> PASS")
    else:
        fail_count += 1
        print(f"{cmd:14} -->  {echo:14} -->  {rsp:54} >>>>> FAIL: expected '" + exp + "'")
    return correct, echo, rsp


def one_group(group):
    for t in group:
        flush_in()
        one_test(t[0], t[1])


#port = ps.Serial('COM10', timeout=5)
port = ps.Serial('/dev/ttyACM1', timeout=5)

pass_count = 0
fail_count = 0

time.sleep(1)


for group in tests:
    one_group(group)

print(f"Passed: {pass_count}, Failed: {fail_count}")

port.close()