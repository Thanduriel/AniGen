import os
import argparse

def split_name(name, ani):
	s = name.find(ani)
	return name[0:s], name[s:]

parser = argparse.ArgumentParser(description='Constructs queries for AniGen by scanning available files.')
parser.add_argument('directories', default='.', nargs='+', help='list of directories to scan')
parser.add_argument('-i', '--input_ani', help='name of the reference animation')
parser.add_argument('-t', '--target_ani', help='name of the target animation')
args = parser.parse_args()

input_files = []
target_files = []

# look for candidates
for dir in args.directories:
	for root, dirs, files in os.walk(dir):
		for file in files:
			if args.input_ani in file:
				input_files.append((root,file))
			if args.target_ani in file:
				target_files.append((root,file))

# match pairs
map_pairs = []
obj_names = []
for inp_root, inp_file in input_files:
	s = inp_file.find(args.input_ani)
	obj = inp_file[0:s]
	name = obj + args.target_ani
	matches = list(filter(lambda x: name in x[1], target_files))
	if matches:
		obj_names.append(obj[0:-1])
		map_pairs.append((os.path.join(inp_root, inp_file), os.path.join(matches[0][0], matches[0][1])))

print("Found matching assets for {}.\n".format(obj_names))

command = ""
for input_file, target_file in map_pairs:
	command += "-i {} -t {}".format(input_file, target_file)

print(command)