import os
import argparse

def find_reference_pairs(directories, input_ani, target_ani):
	input_files = []
	target_files = []

	# look for candidates
	for dir in directories:
		for root, dirs, files in os.walk(dir):
			for file in files:
				if input_ani in file:
					input_files.append((root,file))
				if target_ani in file:
					target_files.append((root,file))

	# match pairs
	map_pairs = []
	obj_names = []
	for inp_root, inp_file in input_files:
		s = inp_file.find(input_ani)
		obj = inp_file[0:s]
		name = obj + target_ani
		matches = list(filter(lambda x: name in x[1], target_files))
		if matches:
			obj_names.append(obj[0:-1])
			map_pairs.append((os.path.join(inp_root, inp_file), os.path.join(matches[0][0], matches[0][1])))

	return map_pairs, obj_names

def make_param_list(map_pairs):
	command = ""
	for input_file, target_file in map_pairs:
		command += "-i \"{}\" -t \"{}\" ".format(input_file, target_file)

	return command

if __name__ == "__main__":
	parser = argparse.ArgumentParser(description='Constructs queries for AniGen by scanning for suitable files.')
	parser.add_argument('directories', default='.', nargs='+', help='list of directories to scan')
	parser.add_argument('-i', '--input_ani', help='name of the reference animation')
	parser.add_argument('-t', '--target_ani', help='name of the target animation')
	args = parser.parse_args()

	map_pairs,obj_names = find_reference_pairs(args.directories, args.input_ani, args.target_ani)

	print("Found matching assets for {}.\n".format(obj_names))
	print(make_param_list(map_pairs))