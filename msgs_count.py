import sys
from dateutil import parser
from datetime import datetime, timedelta
from random import choice

# RATE = float(sys.argv[3])
RATE = -1.0

def find_next(rec):
    max_key, max_val = -1, -1
    for key, val in rec.items():
        if max_val == val:
            return -1
        if max_val < val:
            max_key, max_val = key, val
    # print RATE, "kek"
    for key, val in rec.items():
        if max_val != val and max_val * RATE <= val:
            return -1

    return max_key


def find_source(events):
    cnt = {}
    for event in events:
        f, t = event["from"], event["to"]
        # if f == 1:
        #     print t
        if t not in cnt:
            cnt[t] = {}
        if f not in cnt[t]:
            cnt[t][f] = 0
        cnt[t][f] += 1

    cur_node = 1
    was = False
    while(True):
        if cur_node not in cnt:
            break
        next_node = find_next(cnt[cur_node])
        if next_node == -1:
            break
        if next_node == 2:
            was = True
        cur_node = next_node
    return cur_node, was

def parse_timedelta(t):
    if len(t.split(':')) <= 2:
        return timedelta(minutes=int(t.split(':')[0]),
                         seconds=int(t.split(':')[1].split('.')[0]),
                         milliseconds=int(t.split('.')[1]))
    else:
        return timedelta(hours=int(t.split(':')[0]),
                         minutes=int(t.split(':')[1]),
                         seconds=int(t.split(':')[2].split('.')[0]),
                         milliseconds=int(t.split('.')[1]))


def iteration(T):
    f = open(sys.argv[1])
    # T = int(sys.argv[2]) # T in seconds
    # RATE = float(sys.argv[3])
    events = []

    for cnt, line in enumerate(f):
        if "Dummy:" in line or "Real:" in line:
            t = line.split('\t')[0]
            event = {"time": parse_timedelta(t),
                     "real": "Real:" in line,
                     "from": int(line.split(' ')[1]),
                     "to": int(line.split(' ')[3])}
            events.append(event)
    # print events[-1]["time"]
    initial_time = choice([x for x in events if events[-1]["time"] - x["time"] >= timedelta(seconds=T)])["time"]
    final_time = initial_time + timedelta(seconds=T)
    sample = [x for x in events if initial_time <= x["time"] <= final_time]
    return len(sample)


def main():
    kek = 0.0
    trials = [100, 500, 1000, 6000, 10000]
    for T in trials:
        kek = 0.0
        for i in range(100):
            kek += iteration(T)
        print ("For T = %d avg len = %2f" % (T, kek / 100))

if __name__=="__main__":
    main()

# final_time = events[0]["time"] + timedelta(seconds=T)
# print events[0]["time"], final_time
#
#
#
# for event in events:
#     if event["time"] > final_time:
#         break
#     print event
