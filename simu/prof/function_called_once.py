import json
import re
import numpy as numpy

if __name__ == '__main__':
  with open('./simu_timeline.json', 'r', encoding='utf8')as fp:
    json_data = json.load(fp)

  trace_evt = json_data['traceEvents']
  parallel_dict = {}

  #orgnize trace_event by parallel_cnt
  for list_value in trace_evt:
    parallel_cnt =  list_value['args']['parallel_cnt']
    if re.match(' pint_move_event', list_value['name']) is not None or \
      re.match(' pint_thread', list_value['name']) is not None or \
        re.match(' pint_lpm', list_value['name']) is not None:
      parallel_dict.setdefault(parallel_cnt, []).append(list_value)
  
  # print(parallel_dict)

  onetime_funcname = []
  for value in parallel_dict.values():
    if len(value) <= 1:
      onetime_funcname.append(value[0]['name'])
  # print(onetime_funcname)
  
  
  onetime_funcname_remove_duplicate = list(set(onetime_funcname)) 

  csv_file = open("function_called_once.csv", "w")
  csv_file.write("function name; occur times;\n")
  for item in onetime_funcname_remove_duplicate:
    csv_file.write("%s; %d;\n" %(item, onetime_funcname.count(item)))
    # print("%s; %d;\n" %(item, onetime_funcname.count(item)))
  csv_file.close()
  print("the result is in function_called_once.csv")

