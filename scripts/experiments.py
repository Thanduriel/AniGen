import scan
import subprocess
import time
from prettytable import PrettyTable
from collections import namedtuple

import numpy as np
from geneticalgorithm import geneticalgorithm as ga

similarity_measures = ["identity 1 x 1 1;",
					   "equality 3 x 3 1 1 1; 1 1 1; 1 1 1;",
					   "equality 3 x 3 1 1 1; 1 3 1; 1 1 1;",
			#		   "equality 3 x 3 1 1 1; 1 11 1; 1 1 1;",
		#			   "equality 3 x 3 0 1 0; 1 1 1; 0 1 0;",
		#			   "equality 3 x 3 1 0 1; 0 1 0; 1 0 1;",
		#			   "blur 3 x 3 1 1 1; 1 1 1; 1 1 1;",
		#			   "blur 3 x 3 1 1 1; 1 11 1; 1 1 1;",
		#			   "blur 3 x 3 1 0 1; 0 1 0; 1 0 1;",
					   "equality 3 x 3 0.845 0.529 0.732; 0.561 0.754 0.367; 0.758 0.141 0.243;",
		#			   "equalityrotinv 3 x 3 1 1 1; 1 3 1; 1 1 1;",
					   "equalityrotinv 3 x 3 0.90553009 0.51973339 0.56880356 0.73786717 0.76955676 0.0523793 0.60405608 0.30422786 0.39515804;",
		#			   "minblur 3 x 3 1 1 1; 1 3 1; 1 1 1;",
		#			   "minequalityrotinv 3 x 3 1 1 1; 1 3 1; 1 1 1;",
		]

# input name, input num frames, reference frame, target name, num target frames
maps = [("Combat_Hit", 1, 0, "Block_1H_Rtrn", 1),
	("Atk_1H_00_Hold", 2, 1, "Block_1H_Rtrn", 1),
	("Combat_Hit", 1, 0, "Normal_Walk", 8),
	("Combat_Hit", 1, 0, "Male_Dodge_Forward", 5)]

directories = [
	"../Sprites/FullyAnimated/Chest_OrderArmor_Heavy/",
	"../Sprites/FullyAnimated/Chest_OrderArmor_Light/",
	"../Sprites/FullyAnimated/Template_Muscular_Black/",
	"../Sprites/PartiallyAnimated/Chest_FamilyMedium/", # mostly complete
	"../Sprites/FullyAnimated/Template_Muscular_White/",
	"../Sprites/PartiallyAnimated/Chest_Hunter/",
	"../Sprites/PartiallyAnimated/Chest_LeatherArmor_Torn/",
	"../Sprites/PartiallyAnimated/Chest_OrderArmor_Medium/", # missing many anis
	"../Sprites/PartiallyAnimated/Chest_Linen/",
	"../Sprites/PartiallyAnimated/Feet_LeatherShoes/",
	"../Sprites/PartiallyAnimated/Feet_FurBoots/",
	"../Sprites/FullyAnimated/Feet_OrderShoes/",
	]

zone_map = ("\"../Sprites/Demo/Mask_Combat_Hit.png\"", "\"../Sprites/Demo/Mask_Block_1H_Rtrn.png\"")

def run_experiment(map_args, ref_set, test_set, similarity_measure,
	zone_map=None,
	show_cmd_outputs=False,
	crop_border=0):
	# create map
	zone_map_cmd = ""
	if zone_map:
		zone_map_cmd = "-i {} -t {} -z".format(zone_map[0], zone_map[1])
	arg_list = scan.make_arg_list(ref_set)
	command = "AniGen create {} {} -n {} -m {} -r {} -s \"{}\" -o \"test.map\" -j 4 --crop {}".format(
		zone_map_cmd,
		arg_list, 
		map_args[1],
		map_args[4],
		map_args[2],
		similarity_measure,
		crop_border)

	start = time.time()
	subprocess.run(command, check=True, capture_output=not show_cmd_outputs)
	end = time.time()
	
	# apply map
	result_sum = [0.0, 0.0]
	for test_pair in test_set:
		command = "AniGen apply -i {} -n {} -r {} -t test.map -o sprite".format(
			test_pair[0],
			map_args[1], 
			map_args[2])
		subprocess.run(command, check=True, capture_output=not show_cmd_outputs)
	
		# evaluate result
		command = "AniGen evaluate -i \"{}\" -t sprite_test.png".format(test_pair[1])
		result = subprocess.run(command, check=True, capture_output=True, text=True)
		result = list(map(float, result.stdout.split(",")))
		for i in range(0, len(result_sum)):
			result_sum[i] += result[i]
	for i in range(0, len(result_sum)):
		result_sum[i] /= len(test_pair)
	return result_sum, end-start

def unpack_experiment(experiment):
	name, map_args, ref_set, test_set, similarity_measure, zone_map = experiment
	if isinstance(map_args,int): 
		map_args = maps[map_args]
	if isinstance(similarity_measure,int): 
		similarity_measure = similarity_measures[similarity_measure]
	if isinstance(ref_set[0], int):
		dirs = [directories[i] for i in ref_set]
		ref_set, _ = scan.find_reference_pairs(dirs, map_args[0], map_args[3])
	if isinstance(test_set[0], int):
		dirs = [directories[i] for i in test_set]
		test_set, _ = scan.find_reference_pairs(dirs, map_args[0], map_args[3])

	return Experiment(name,map_args,ref_set,test_set, similarity_measure, zone_map)

# run a list of experiments and generate a result table
def make_table(experiments):
	results_table = PrettyTable()
	results_table.field_names = ["name", "0-error", "1-error"]

	for exp in experiments:
		name, map_args, ref_set, test_set, similarity_measure, zone_map = unpack_experiment(exp)
		err, duration = run_experiment(map_args, 
									ref_set, 
									test_set, 
									similarity_measure, 
									zone_map,
									show_cmd_outputs=False,
									crop_border=2)
		results_table.add_row([name, err[0], err[1]])

	return results_table

Experiment = namedtuple('Experiment', 'name map reference_set test_set similarity_measure zone_map')

exp_similarity_optim = [
		Experiment("unnamed", 0, [0,1,6], [3], 0, 0),
		Experiment("unnamed", 0, [5,3,6], [0], 0, 0),
		Experiment("unnamed", 0, [9,10], [11], 0, 0),
		Experiment("unnamed", 2, [0,1,6], [3], 0, 0),
		Experiment("unnamed", 2, [5,3,6], [0], 0, 0),
		Experiment("unnamed", 2, [9,10], [11], 0, 0)
	]

kernel_size = 3
def goal_fn(X):
	err0 = 0
	err1 = 0
	similarity_measure = "equalityrotinv {} x {}".format(kernel_size, kernel_size)
	for i in range(0, len(X)):
		similarity_measure += " {}".format(X[i])
		if (i+1) % kernel_size == 0:
			similarity_measure += ";"

	for exp in exp_similarity_optim:
		_, map_ind, ref_set, test_set, _, zone_map = unpack_experiment(exp)
		err, _ = run_experiment(maps[map_ind], 
								ref_set, 
								test_set, 
								similarity_measure, 
								zone_map,
								crop_border=kernel_size//2+1)
		err0 += err[0]
		err1 += err[1]

	return err0 + err1

def run_optimization():
	dim = kernel_size * kernel_size
	varbound = np.array([[0,1]]*dim)
	algorithm_param = {'max_num_iteration': 100,\
					   'population_size':100,\
					   'mutation_probability':0.1,\
					   'elit_ratio': 0.02,\
					   'crossover_probability': 0.5,\
					   'parents_portion': 0.3,\
					   'crossover_type':'uniform',\
					   'max_iteration_without_improv':None}

	model=ga(function=goal_fn,
			dimension=dim,
			variable_type='real',
			variable_boundaries=varbound,
			algorithm_parameters=algorithm_param,
			function_timeout=20)
	model.run()

	print(model.report)
	print(model.output_dict)

def main():
	experimentsBasic = [
		Experiment("template 1x1", 0, [2], [4], 0, None)
	]

	map_ind = 2
	similarity = 1
	use_zone_map = None
	exp_num_inputs = [
		Experiment("1", map_ind, [0], [3], similarity, use_zone_map),
		Experiment("1", map_ind, [1], [3], similarity, use_zone_map),
		Experiment("2", map_ind, [0,1], [3], similarity, use_zone_map),
		Experiment("3 template", map_ind, [0,1,2], [3], similarity, use_zone_map),
		Experiment("3", map_ind, [0,1,6], [3], similarity, use_zone_map),
		Experiment("4", map_ind, [0,1,2,6], [3], similarity, use_zone_map),
		Experiment("5", map_ind, [0,1,2,6,5], [3], similarity, use_zone_map)
	]

	exp_similarity = []
	for i in range(0,len(similarity_measures)):
		exp_similarity.append(Experiment(similarity_measures[i], 2, [0,1,2,6,5], [3], i, None))

	#print(make_table(experimentsBasic))
	print(make_table(exp_num_inputs))
	#print(make_table(exp_similarity))

	#print(make_table(exp_similarity_optim))
	#run_optimization()
	#print(make_table([Experiment("holes", 2, [0,1,2,5,6], [3], 2, 0)]))

	#make_table([exp_similarity[2]])

if __name__ == "__main__":
	main()