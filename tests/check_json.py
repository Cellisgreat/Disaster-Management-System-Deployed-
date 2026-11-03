#!/usr/bin/env python3
"""tests/check_json.py — assertions over API responses for acceptance.sh

Keeps the shell test script free of fragile inline quoting. Each check prints
one value (or True/False) that the shell script compares.

Usage:
    python3 tests/check_json.py <response.json> <check> [extra args]

Checks
    local-first           -> True when volunteers in the affected area come first
    distance-sorted       -> True when out-of-area volunteers are nearest-first
    no-double-booking     -> True when no volunteer is on two active requests
    available-and-assigned-> number of volunteers both available and assigned
    field <a> <b> ...     -> prints the nested field value (e.g. field stats volunteers total)
    count <list-key>      -> number of items in a top-level list
"""
import json
import sys


def load(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def allocations(data):
    return data.get("allocations", [])


def check_local_first(data):
    items = allocations(data)
    seen_remote = False
    for item in items:
        if item.get("local"):
            if seen_remote:
                return False
        else:
            seen_remote = True
    return True


def check_distance_sorted(data):
    items = allocations(data)
    previous = None
    for item in items:
        if item.get("local"):
            continue
        distance = float(item.get("distanceKm", 0))
        if previous is not None and distance < previous - 1e-9:
            return False
        previous = distance
    return True


def check_no_double_booking(data):
    ids = []
    for request in data.get("requests", []):
        if request.get("status") != "Active":
            continue
        for volunteer in request.get("assignedVolunteers", []):
            ids.append(volunteer.get("id"))
    return len(ids) == len(set(ids))


def count_available_and_assigned(data):
    bad = 0
    for volunteer in data.get("volunteers", []):
        if volunteer.get("available") and volunteer.get("assignmentCount", 0) > 0:
            bad += 1
    return bad


def main(argv):
    if len(argv) < 3:
        print("usage: check_json.py <json-file> <check> [args]", file=sys.stderr)
        return 2
    path, check = argv[1], argv[2]
    data = load(path)

    if check == "local-first":
        print("True" if check_local_first(data) else "False")
    elif check == "distance-sorted":
        print("True" if check_distance_sorted(data) else "False")
    elif check == "no-double-booking":
        print("True" if check_no_double_booking(data) else "False")
    elif check == "available-and-assigned":
        print(count_available_and_assigned(data))
    elif check == "count":
        print(len(data.get(argv[3], [])))
    elif check == "field":
        node = data
        for key in argv[3:]:
            if isinstance(node, list):
                node = node[int(key)]
            else:
                node = node[key]
        print(node)
    else:
        print("unknown check: " + check, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
