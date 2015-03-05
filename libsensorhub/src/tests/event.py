#!/usr/bin/python
import os

class subevent ():
	def __init__ (self):
		self.sensor = -1
		self.chan   = -1
		self.opt    = -1	
		self.param1 = 0
		self.param2 = 0
			
	def __str__(self):
		assert self.sensor != -1
		assert self.chan != -1
		assert self.opt != -1
		pre = "%d %d %d "%(self.sensor, self.chan, self.opt)
		return pre + str(self.param1) + " " + str(self.param2)

class event ():
	def __init__(self):
		self.subs = []
		self.rel  = -1

	def evt_part(self):
		subevt_num = len(self.subs)
		return "%d %d "%(subevt_num, self.rel)

	def subevt_part(self):
		ret = ""
		for i in self.subs:
			ret += "%s "%str(i)
		return ret
		
	def __str__(self):
		assert self.rel != -1
		ret = ""
		evt_str = self.evt_part()
		subevt_str = self.subevt_part()
		return ret + evt_str + subevt_str

def test():
	e = event()
	e.rel = 1
	sub1 = subevent()
	sub2 = subevent()

	sub1.sensor = 1
	sub1.chan   = 1
	sub1.opt    = 1
        sub1.param1 = 1
	sub1.param2 = 4

	sub2.sensor = 2
        sub2.chan   = 3
        sub2.opt    = 3
        sub2.param1 = 7
        sub2.param2 = 6

        e.subs.append(sub1)
	e.subs.append(sub2)

	print e

sensor_list = {'accel':0, 
		'gyro':1, 
		'comp':2, 
		'baro':3,
		'als' :3,
		'ps'  :5,
		'term':6,}

chan_list = {'x':1, 
		'y':2, 
		'z':4, 
		'a':7}

opt_list = {'>':1, 
		'<':2, 
		'=':0, 
		'|':3}

def line2evt(line):
	part = line.split(" ")
	assert len(part) > 3
	assert part[0] in sensor_list.keys()
	assert part[1] in chan_list.keys()
	assert part[2] in opt_list.keys()

	if part[2] == '|':
		assert len(part) == 5

	e = subevent()
	e.sensor = sensor_list[part[0]]
	e.chan	= chan_list[part[1]]
	e.opt = opt_list[part[2]]
	e.param1 = int(part[3])
	
	if len(part) == 5:
		e.param2 = int(part[4])

	return e	
	

def p(s):
	lines = [i.strip() for i in s.splitlines()]
	i = len(lines)

	e = event()
	
	if i != 1:
		t = lines[-1]
		assert t in ['&' ,'|']
		e.rel = {'&':0, '|':1}[t]
		lines = lines[:-1]		
	else:
		e.rel = 0 # Default relation

	for i in lines:
		e.subs.append(line2evt(i))
	
	return e

evt_cmd = 'event_notification'

pre = '' #'echo'
debug = 0

def send_cmd(s):
	cmd = "adb shell " + s
	print cmd
	if not debug:
		os.system(cmd)

def evt(s):
	s = p(s).__str__()
	send_cmd("%s %s \"%s\""%(pre, evt_cmd, s))

if __name__ == '__main__':
	import sys
	lines = sys.stdin.readlines()
	if not lines:
		exit()

	evt("".join(lines))
#	test()
