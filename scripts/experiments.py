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
					   "equality 3 x 3 1 1 1; 1 11 1; 1 1 1;",
					   "equality 3 x 3 0 1 0; 1 1 1; 0 1 0;",
					   "equality 3 x 3 1 0 1; 0 1 0; 1 0 1;",
					   "blur 3 x 3 1 1 1; 1 1 1; 1 1 1;",
					   "blur 3 x 3 1 1 1; 1 11 1; 1 1 1;",
					   "blur 3 x 3 1 0 1; 0 1 0; 1 0 1;",]

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
	zone_map=None):
	# create map
	zone_map_cmd = ""
	if zone_map:
		zone_map_cmd = "-i {} -t {} -z".format(zone_map[0], zone_map[1])
	dirs = [directories[i] for i in ref_set]
	map_pairs, _ = scan.find_reference_pairs(dirs, map_args[0], map_args[3])
	param_list = scan.make_param_list(map_pairs)
	command = "AniGen create {} {} -n {} -m {} -r {} -s \"{}\" -o \"test.map\" -j 4".format(
		zone_map_cmd,
		param_list, 
		map_args[1], 
		map_args[4], 
		map_args[2],
		similarity_measure)

	start = time.time()
	subprocess.run(command, check=True, capture_output=True)
	end = time.time()
	
	# apply map
	dirs = [directories[i] for i in test_set]
	map_pairs, _ = scan.find_reference_pairs(dirs, map_args[0], map_args[3])
	command = "AniGen apply -i {} -n {} -m {} -r {} -t test.map -o sprite".format(
		map_pairs[0][0],
		map_args[1], 
		map_args[4], 
		map_args[2],)
	subprocess.run(command, check=True, capture_output=True)
	
	# evaluate result
	command = "AniGen evaluate -i \"{}\" -t sprite_test.png".format(map_pairs[0][1])
	result = subprocess.run(command, check=True, capture_output=True, text=True)

	return list(map(float, result.stdout.split(","))), end-start

# run a list of experiments and generate a result table
def make_table(experiments):
	results_table = PrettyTable()
	results_table.field_names = ["name", "inputs", "0-error", "1-error"]

	for name, map_ind, ref_set, test_set, similarity_measure, zone_map in experiments:
		err, duration = run_experiment(maps[map_ind], 
									ref_set, 
									test_set, 
									similarity_measures[similarity_measure], 
									zone_map)
		results_table.add_row([name, len(ref_set), err[0], err[1]])

	return results_table
#for map_args in maps:
#	for ref_set,test_set in zip(reference_sets, test_sets):
#		for similarity_measure in similarity_measures:
#			err, duration = run_experiment(map_args, ref_set, test_set, similarity_measure)
#			results_table.add_row(["armor", len(ref_set), err[0], err[1]])


Experiment = namedtuple('Experiment', 'name map reference_set test_set similarity_measure zone_map')
experimentsBasic = [
	Experiment("template 1x1", 0, [2], [4], 0, None)
	]

map_ind = 0
similarity = 1
use_zone_map = zone_map
exp_num_inputs = [
	Experiment("1", map_ind, [0], [3], similarity, zone_map),
	Experiment("1", map_ind, [1], [3], similarity, use_zone_map),
	Experiment("2", map_ind, [0,1], [3], similarity, use_zone_map),
	Experiment("3 template", map_ind, [0,1,2], [3], similarity, use_zone_map),
	Experiment("3", map_ind, [0,1,6], [3], similarity, use_zone_map),
	Experiment("4", map_ind, [0,1,2,6], [3], similarity, use_zone_map),
	Experiment("5", map_ind, [0,1,2,6,5], [3], similarity, use_zone_map)
	]

exp_similarity = []
for i in range(0,len(similarity_measures)):
	exp_similarity.append(Experiment(similarity_measures[i], 0, [0,1,6], [3], i, None))

exp_similarity_optim = [
		Experiment("unnamed", 0, [0,1,6], [3], 1, 0),
		Experiment("unnamed", 0, [5,3,6], [0], 1, 0),
		Experiment("unnamed", 0, [9,10], [11], 1, 0),
		Experiment("unnamed", 2, [0,1,6], [3], 1, 0),
		Experiment("unnamed", 2, [5,3,6], [0], 1, 0),
		Experiment("unnamed", 2, [9,10], [11], 1, 0)
	]
#print(make_table(experimentsBasic))
#print(make_table(exp_num_inputs))
#print(make_table(exp_similarity))

#print(make_table(exp_similarity_optim))

kernel_size = 3
def goal_fn(X):
	err0 = 0
	err1 = 0
	similarity_measure = "blur {} x {}".format(kernel_size, kernel_size)
	for i in range(0, len(X)):
		similarity_measure += " {}".format(X[i])
		if (i+1) % kernel_size == 0:
			similarity_measure += ";"

	for name, map_ind, ref_set, test_set, _, zone_map in exp_similarity_optim:
		err, _ = run_experiment(maps[map_ind], 
								ref_set, 
								test_set, 
								similarity_measure, 
								zone_map)
		err0 += err[0]
		err1 += err[1]

	return err0 + err1

dim = kernel_size * kernel_size
varbound = np.array([[0,1]]*dim)
algorithm_param = {'max_num_iteration': 16,\
                   'population_size':32,\
                   'mutation_probability':0.1,\
                   'elit_ratio': 0.01,\
                   'crossover_probability': 0.5,\
                   'parents_portion': 0.3,\
                   'crossover_type':'uniform',\
                   'max_iteration_without_improv':None}

model=ga(function=goal_fn,
		dimension=dim,
		variable_type='real',
		variable_boundaries=varbound,
		algorithm_parameters=algorithm_param)
model.run()

print(model.report)
print(model.ouput_dict)