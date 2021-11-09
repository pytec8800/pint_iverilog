import json
import numpy as numpy

if __name__ == '__main__':
  with open('./simu_timeline.json', 'r', encoding='utf8')as fp:
    json_data = json.load(fp)

  trace_evt = json_data['traceEvents']
  parallel_dict = {}

  #orgnize trace_event by parallel_cnt
  for list_value in trace_evt:
    parallel_cnt =  list_value['args']['parallel_cnt']
    if list_value['name'] == " execute_active_q_thread()":
      parallel_dict.setdefault(parallel_cnt, []).append(list_value)

  total_idle_time = 0
  total_ncore_time = 0
  for value in parallel_dict.values():
    dur_list = []
    for item in value:
      dur_list.append(item["dur"])
    max_time = max(dur_list)
#    print("max_time: {}".format(max_time))
    total_ncore_time += max_time*16
    idle_time = 0
    for item in dur_list:
      idle_time += (max_time - item)
#    print("idle_time: {}".format(idle_time))
    total_idle_time += idle_time


  ncore_occupancy = (total_ncore_time - total_idle_time) / total_ncore_time * 100
  print("total_idle_time: {}.".format(int(total_idle_time*1000)))
  print("total_ncore_time: {}.".format(int(total_ncore_time*1000)))
  print("ncore_occupancy: {}%.".format(ncore_occupancy))


  
