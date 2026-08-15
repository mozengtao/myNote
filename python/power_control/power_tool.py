#!/usr/bin/env python3

import argparse
import sys
import requests
from power_bar_library import LPC
from requests.auth import HTTPDigestAuth
from pathlib import Path

import yaml

SCRIPT_DIR = Path(__file__).resolve().parent


def parse_args():
	parser = argparse.ArgumentParser(description='Control power for RPDs.')
	parser.add_argument(
		'rpd',
		help='RPD name or "all" to target every RPD',
	)
	parser.add_argument(
		'action',
		choices=['cycle', 'on', 'off'],
		help='Power action to perform',
	)
	return parser.parse_args()


def load_data():
	with open(SCRIPT_DIR / 'power_data.yaml', 'r') as file:
		return yaml.safe_load(file)

def issue_power_command(power_device, outlet, action):

	if action == 'CCL':
		power_device.cycle_outlet(outlet)
	elif action == 'ON':
		power_device.turn_on_outlet(outlet)
	elif action == 'OFF':
		power_device.turn_off_outlet(outlet)
	else:
		print(f'Unknown action: {action}', file=sys.stderr)
		sys.exit(1)

	print(f'Issued power command to {power_device.ip} for outlet {outlet}')


def do_power_action(rpd, action):
	cycle_action = None
	if action == 'cycle':
		print(f'Cycling power for {rpd["name"]}')
		cycle_action = 'CCL'
	elif action == 'on':
		print(f'Turning on power for {rpd["name"]}')
		cycle_action = 'ON'
	elif action == 'off':
		print(f'Turning off power for {rpd["name"]}')
		cycle_action = 'OFF'
	else:
		print(f'Unknown action: {action}', file=sys.stderr)
		sys.exit(1)

	for power_server in rpd['power']:
		power_device = LPC(user_name=power_server['user'], ip_address=power_server['address'], password=power_server['password'])
		for outlet in power_server['modem_outlets']:
			issue_power_command(power_device, outlet, cycle_action)
		power_device.logout()

def main():
	args = parse_args()
	data = load_data()

	if 'rpds' not in data:
		print('No rpds found in power_data.yaml', file=sys.stderr)
		sys.exit(1)

	rpds = data['rpds']
	if args.rpd != 'all':
		looked_up_rpd = [rpd for rpd in rpds if rpd['name'] == args.rpd]
		if len(looked_up_rpd) == 0:
			print(f'Unknown RPD: {args.rpd}', file=sys.stderr)
			sys.exit(1)

		do_power_action(looked_up_rpd[0], args.action)

	else:
		print("Performing {args.action} for all RPDs")
		for rpd in rpds:
			do_power_action(rpd, args.action)


if __name__ == '__main__':
	main()
