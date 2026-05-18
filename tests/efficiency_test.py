import midas.file_reader
import matplotlib.pyplot as plt
import numpy as np

# Configure
PRINT_DATA = False
CUTOFF_COUNT = 5000000000
START_COUNT = 5
RAW_BINS = 80000
DECIMATION = 2
EXPECTED_DIFF = 16000000
TIMESTAMP_BANK_NAME = 'Ts00'
FILENAME = "/work/mancx/mattm/MANCX/online/run00162.mid.lz4"

mfile = midas.file_reader.MidasFile(FILENAME)

count = 0
t0 = 0
tN = 0
tprev = 0
same_count = 0
timestamps = []
for event in mfile:

    count += 1
    if count >= CUTOFF_COUNT: break

    if event.header.is_midas_internal_event():
        print("Saw a special event")
        count -= 1
        continue
    
    bank_names = ", ".join(b.name for b in event.banks.values())
    if PRINT_DATA:
        print("Event # %s of type ID %s contains banks %s" % (event.header.serial_number, event.header.event_id, bank_names))

    for bank_name, bank in event.banks.items():
        # bank.data is generally a python tuple.
        # If use_numpy=True was specified when opening the MidasFile, then
        # bank.data is a numpy array.
        if PRINT_DATA:
            if len(bank.data):
                print("    The first entry in bank %s is %s" % (bank_name, bank.data[0]))
                #print("    The full dataset in bank %s is %s" % (bank_name, bank.data))
                print("    The full data set of bank %s has length %s" % (bank_name, len(bank.data)))

        if not len(bank.data):
            count -= 1

        if count == 2 and len(bank.data) and bank_name == TIMESTAMP_BANK_NAME:
            t0 = bank.data[0]
        if len(bank.data) and bank_name == TIMESTAMP_BANK_NAME:
            tN = bank.data[0]
            if tprev == tN:
                same_count += 1
            tprev = tN
            timestamps.append(tN)


timestamps = np.array(timestamps)
unique_ts, unique_i = np.unique(timestamps, return_index=True)
n_acquisitions = len(unique_ts)
same_count = count - n_acquisitions

expected_samples = (count - same_count) * RAW_BINS * DECIMATION
observed_samples = expected_samples

huge_diff_i = 0

print("\nTimestamp analysis:")
for i in range(len(unique_ts) - 2):
    diff = unique_ts[i+1] - unique_ts[i]
    if diff != EXPECTED_DIFF:
        print(f'Consecutive difference: {diff}')
        print(f'index: {i}')
        print(f'ts[{i-2}] = {unique_ts[i-2]}')
        print(f'ts[{i-1}] = {unique_ts[i-1]}')
        print(f'ts[{i}] = {unique_ts[i]}')
        print(f'ts[{i+1}] = {unique_ts[i+1]}')
        print(f'ts[{i+2}] = {unique_ts[i+2]}')
        

        if diff < 1000 * EXPECTED_DIFF and i > CUTOFF_COUNT:
            observed_samples = observed_samples - EXPECTED_DIFF + diff
        else:
            huge_diff_i = i

print('')

efficiency = expected_samples / observed_samples * 100

print(f'timestamp 0:                {t0}')
print(f'timestamp N:                {tN}')
print(f'Unique timestamp:           {n_acquisitions}')
print(f'Number of events in file:   {count}')
print(f'Same counts in file:        {same_count}')
print(f'Number of samples in file:  {observed_samples}')
print(f'Number of expected samples: {expected_samples}')
print(f'Efficiency:                 {efficiency:.3f}%')
