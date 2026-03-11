import time
import serial as ps

track_tests = [
    # track_msg
    [ 'T',             'ERROR'   ],
    [ 'T 0',           'ERROR'   ],
    [ 'T X',           'ERROR'   ],
    # track_get_msg, track_set_msg
    [ 'T G 0',         'ERROR'   ],
    [ 'T S 1',         'OK'      ],
    [ 'T G',           'OK 1'    ],
    [ 'T S 0',         'OK'      ],
    [ 'T G',           'OK 0'    ],
    # track_set_msg
    [ 'T S',           'ERROR'   ],
    [ 'T S 0 0',       'ERROR'   ],
    [ 'T S X',         'ERROR'   ],
    [ 'T S 2',         'ERROR'   ],
]

cv_tests = [
    # Note: These tests assume a loco on the track
    # setup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
    # cv_msg
    [ 'C',             'ERROR'   ],
    [ 'C A',           'ERROR'   ],
    [ 'C 0',           'ERROR'   ],
    [ 'C 1025',        'ERROR'   ],
    [ 'C 8',           'ERROR'   ],
    [ 'C 8 0',         'ERROR'   ],
    # cv_get_msg
    [ 'C 8 G 0',       'ERROR'   ],
    [ 'C 3 S 85',      'OK'      ],
    [ 'C 3 G',         'OK 85'   ],
    # cv_set_msg
    [ 'C 3 S',         'ERROR'   ],
    [ 'C 3 S 0 0',     'ERROR'   ],
    [ 'C 3 S X',       'ERROR'   ],
    [ 'C 3 S 256',     'ERROR'   ],
    # cv_bit_msg
    [ 'C 3 B',         'ERROR'   ],
    [ 'C 3 B X',       'ERROR'   ],
    [ 'C 3 B 8',       'ERROR'   ],
    [ 'C 3 B 0',       'ERROR'   ],
    [ 'C 3 B 0 0',     'ERROR'   ],
    [ 'C 3 B 0 X',     'ERROR'   ],
    # cv_bit_get_msg
    [ 'C 3 B 0 G 0',   'ERROR'   ],
    [ 'C 3 S 85',      'OK'      ],
    [ 'C 3 B 0 G',     'OK 1'    ],
    [ 'C 3 B 1 G',     'OK 0'    ],
    # cv_bit_set_msg
    [ 'C 3 B 0 S',     'ERROR'   ],
    [ 'C 3 B 0 S 1 0', 'ERROR'   ],
    [ 'C 3 B 0 S X',   'ERROR'   ],
    [ 'C 3 B 0 S 2',   'ERROR'   ],
    [ 'C 3 B 0 S 0',   'OK'      ],
    [ 'C 3 G',         'OK 84'   ],
    # cleanup
    [ 'C 8 S 8',       'OK'      ], # reset loco
]

address_tests = [
    # setup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
    # address_msg
    [ 'A',             'ERROR'   ],
    [ 'A 0',           'ERROR'   ],
    [ 'A X',           'ERROR'   ],
    # address_get_msg, address_set_msg
    [ 'A S 3',         'OK'      ],
    [ 'A G',           'OK 3'    ],
    [ 'C 29 B 5 G',    'OK 0'    ],
    [ 'A S 1234',      'OK'      ],
    [ 'A G',           'OK 1234' ],
    [ 'C 29 B 5 G',    'OK 1'    ],
    # address_get_msg
    [ 'A G X',         'ERROR'   ],
    # address_set_msg
    [ 'A S 3 3',       'ERROR'   ],
    [ 'A S X',         'ERROR'   ],
    [ 'A S 0',         'ERROR'   ],
    [ 'A S 11000',     'ERROR'   ],
    # cleanup
    [ 'C 8 S 8',       'OK'      ], # reset loco
]


loco_tests = [
    # setup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
    # loco_msg
    [ 'L',             'ERROR'   ],
    [ 'L X',           'ERROR'   ],
    [ 'L 0',           'ERROR'   ],
    [ 'L 11000',       'ERROR'   ],
    [ 'L 3',           'ERROR'   ],
    [ 'L 3 0',         'ERROR'   ],
    # loco_new_msg
    [ 'L 3 N 1',       'ERROR'   ],
    [ 'L 3 N',         'OK'      ],
    [ 'L 3 N',         'ERROR'   ],
    # loco_del_msg
    [ 'L 3 D 1',       'ERROR'   ],
    [ 'L 3 D',         'OK'      ],
    [ 'L 3 D',         'ERROR'   ],
    # cleanup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
]

loco_func_tests = [
    # setup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
    [ 'L 3 N',         'OK'      ], # create loco 3
    # loco_func_msg
    [ 'L 3 F 0 G',     'ERROR'   ],
    [ 'L 3 N',         'OK'      ],
    [ 'L 3 F',         'ERROR'   ],
    [ 'L 3 F X',       'ERROR'   ],
    [ 'L 3 F 100',     'ERROR'   ],
    [ 'L 3 F 0',       'ERROR'   ],
    [ 'L 3 F 0 0',     'ERROR'   ],
    [ 'L 3 F 1 0',     'ERROR'   ],
    [ 'L 3 F 0 G 0',   'ERROR'   ],
    [ 'L 3 F 0 G',     'OK 0'    ],
    [ 'L 3 F 0 S 1',   'OK'      ],
    [ 'L 3 F 0 G',     'OK 1'    ],
    [ 'L 3 F 0 S 0',   'OK'      ],
    [ 'L 3 F 0 G',     'OK 0'    ],
    [ 'L 3 F 0 S 0 0', 'ERROR'   ],
    [ 'L 3 F 0 S N',   'ERROR'   ],
    [ 'L 3 F 0 S 2',   'ERROR'   ],
    [ 'L 4 F 0 S 1',   'ERROR'   ],
    # cleanup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
    [ 'L 3 D',         'OK'      ], # delete loco 3
]

loco_speed_tests = [
    # setup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
    [ 'L 3 N',         'OK'      ], # create loco 3
    # loco_speed_msg: no loco
    [ 'L 4 S G',       'ERROR'   ], # no loco 4
    # loco_speed_msg: missing/bad subcmd
    [ 'L 3 S',         'ERROR'   ], # missing subcmd (argc < 4)
    [ 'L 3 S 0',       'ERROR'   ], # subcmd is INT, not CHAR
    [ 'L 3 S X',       'ERROR'   ], # subcmd unrecognized
    # loco_speed_get_msg ('G')
    [ 'L 3 S G 0',     'ERROR'   ], # extra arg
    [ 'L 3 S G',       'OK 0'    ], # get speed (initially 0)
    # loco_speed_set_msg ('S')
    [ 'L 3 S S',       'ERROR'   ], # missing speed arg
    [ 'L 3 S S X',     'ERROR'   ], # speed is CHAR, not INT
    [ 'L 3 S S -128',  'ERROR'   ], # speed below speed_min (-127)
    [ 'L 3 S S 128',   'ERROR'   ], # speed above speed_max (127)
    [ 'L 3 S S 10 0',  'ERROR'   ], # extra arg
    [ 'L 3 S S 10',    'OK'      ], # set speed to 10
    [ 'L 3 S G',       'OK 10'   ], # verify
    [ 'L 3 S S 0',     'OK'      ], # set speed back to 0
    [ 'L 3 S G',       'OK 0'    ], # verify
    # loco_speed_cb_msg ('R')
    [ 'L 3 S R',       'ERROR'   ], # missing arg
    [ 'L 3 S R X',     'ERROR'   ], # arg is CHAR, not INT
    [ 'L 3 S R 2',     'ERROR'   ], # arg out of range (must be 0 or 1)
    [ 'L 3 S R 1 0',   'ERROR'   ], # extra arg
    [ 'L 3 S R 1',     'OK'      ], # register callback
    [ 'L 3 S R 0',     'OK'      ], # unregister callback
    # cleanup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
    [ 'L 3 D',         'OK'      ], # delete loco 3
]

loco_cv_tests = [
    # setup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
    [ 'L 3 N',         'OK'      ], # create loco 3
    # loco_cv_msg
    [ 'L 3 C 8 G',     'ERROR'   ], # because track is off
    [ 'T S 1',         'OK'      ], # track on (ops mode)
    [ 'L 4 C 8 G',     'ERROR'   ], # no loco 4
    [ 'L 3 C',         'ERROR'   ], # argc < 4
    [ 'L 3 C X',       'ERROR'   ], # argv[3] not an int
    [ 'L 3 C 0',       'ERROR'   ], # argv[3] out of range
    [ 'L 3 C 1025',    'ERROR'   ], # argv[3] out of range
    [ 'L 3 C 8',       'ERROR'   ], # argc < 5
    [ 'L 3 C 8 0',     'ERROR'   ], # argv[4] not a char
    [ 'L 3 C 8 X',     'ERROR'   ], # argv[4] invalid
    # loco_cv_get_msg
    [ 'L 3 C 8 G 0',   'ERROR'   ], # argc != 5
    [ 'L 3 C 8 G',     'OK 151'  ], # read CV8
    # cleanup
    [ 'T S 0',         'OK'      ], # track off (service mode)
    [ 'C 8 S 8',       'OK'      ], # reset loco
    [ 'L 3 D',         'OK'      ], # delete loco 3
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
    #track_tests,
    #cv_tests,
    #address_tests,
    #loco_tests,
    #loco_func_tests,
    loco_speed_tests,
    #loco_cv_tests,
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
        correct = rsp.startswith('rsp: "' + exp)
    global pass_count, fail_count
    if correct:
        pass_count += 1
        print(f"{cmd:16} -->  {echo:16} -->  {rsp:32} >>>>> PASS")
    else:
        fail_count += 1
        print(f"{cmd:16} -->  {echo:16} -->  {rsp:32} >>>>> FAIL: expected '" + exp + "'")
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
    # run in both verbose and non-verbose modes
    one_group(group)

print(f"Passed: {pass_count}, Failed: {fail_count}")

port.close()