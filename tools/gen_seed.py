#!/usr/bin/env python3
"""Generates the seed CSV databases (areas, roads, volunteers, requests, layout).

The topology below is the source of truth for this release: 20 areas, 38 roads.
Run from the project root:  python3 tools/gen_seed.py
"""
import csv, os, sys

AREAS = [
    (0,"Hospital"),(1,"Residential Area"),(2,"School"),(3,"Industrial Area"),
    (4,"Fire Station"),(5,"Central Park"),(6,"City Center"),(7,"Warehouse"),
    (8,"Port"),(9,"Bus Terminal"),(10,"Mountain Area"),(11,"Lake View"),
    (12,"Agricultural Area"),(13,"Airport"),(14,"Tourist Area"),(15,"Beach"),
    (16,"Forest Area"),(17,"Dam"),(18,"Rural Area"),(19,"East Dam"),
]

# (from, to, distance_km, blocked)
ROADS = [
    (0,1,2.5,0),(0,2,3.0,0),(0,6,4.0,0),(0,11,6.5,0),
    (1,5,3.2,0),(1,4,2.8,0),(1,6,3.5,0),
    (2,5,2.2,0),(2,3,5.0,0),(2,12,4.5,0),
    (3,7,2.6,0),(3,9,4.2,0),(3,12,3.4,0),
    (4,8,5.5,0),(4,13,3.8,0),
    (5,6,1.8,0),(5,14,6.0,0),(5,9,3.6,0),
    (6,9,2.4,0),(6,10,7.5,0),
    (7,8,1.9,0),(7,9,3.0,0),
    (8,15,2.7,0),(8,17,4.5,0),
    (9,13,5.2,0),(9,18,6.4,0),
    (10,11,3.6,0),(10,17,5.8,0),(10,16,6.2,0),
    (11,18,4.8,0),(11,12,5.1,0),
    (12,18,3.0,0),
    (13,14,4.0,0),
    (14,15,2.5,0),
    (15,19,6.8,0),
    (16,17,3.1,0),(16,19,4.4,0),
    (17,18,4.0,0),
]

# schematic layout coordinates on a 1000 x 700 canvas
LAYOUT = {
    10:(180,80), 17:(420,80), 19:(690,80), 16:(890,90),
    11:(300,190), 18:(560,190), 12:(800,190),
    0:(150,295), 2:(345,290), 5:(545,290), 3:(785,295),
    1:(150,405), 6:(360,400), 9:(545,400), 7:(785,400),
    4:(150,510), 13:(360,510), 14:(570,510), 8:(800,510),
    15:(620,620),
}

VOLUNTEERS = [
    (1,"Ravi Kumar",8,"Rescue",40,0),
    (2,"Anjali Singh",2,"Food",60,0),
    (3,"Dr. Mehta",0,"Medical Help",30,0),
    (4,"Suresh Yadav",10,"Rescue",25,1),
    (5,"Priya Nair",8,"Rescue",20,0),
    (6,"Imran Sheikh",6,"Food",40,0),
    (7,"Lakshmi Devi",18,"Food",40,0),
    (8,"Dr. Ritu Sharma",6,"Medical Help",50,1),
    (9,"Mohan Das",12,"Food",20,0),
    (10,"Dr. Ayesha Khan",0,"Medical Help",30,1),
    (11,"Vikram Rao",3,"Rescue",35,1),
    (12,"Sunita Patel",5,"Medical Help",25,1),
    (13,"Rajesh Gupta",7,"Rescue",30,1),
    (14,"Neha Verma",13,"Food",45,1),
    (15,"Arjun Reddy",9,"Rescue",50,1),
    (16,"Fatima Noor",14,"Medical Help",20,1),
    (17,"Deepak Mishra",11,"Food",30,1),
    (18,"Kiran Joshi",4,"Rescue",45,1),
    (19,"Sanjay Kumar",15,"Medical Help",35,1),
    (20,"Ananya Iyer",17,"Food",25,1),
]

REQUESTS = [
    (1,"Rescue",8,60,60,[1,5],"Active","Storm surge flooding at the docks. Fishing crews stranded.","citizen.ravi"),
    (2,"Medical Help",1,45,30,[3],"Active","Elderly residents need insulin and BP medicine.","citizen.meera"),
    (3,"Food",12,120,120,[2,6,9],"Active","Food packets required after a crop fire destroyed stores.","citizen.arjun"),
    (4,"Rescue",10,25,25,[4],"Completed","Landslide trapped four hikers on the ridge.","citizen.ravi"),
    (5,"Medical Help",6,80,80,[8,10],"Completed","Multi-vehicle accident triage at the city centre junction.","citizen.fatima"),
    (6,"Food",18,200,40,[7],"Active","Village access road under water, food supplies running out.","citizen.meera"),
]

def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    data = os.path.join(root, "data")
    os.makedirs(data, exist_ok=True)

    with open(os.path.join(data,"areas_db.csv"),"w",newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["id","name"])
        for a in AREAS: w.writerow(a)

    assert len(AREAS) == 20, "areas must be 20"
    assert len(ROADS) == 38, "roads must be 38 (got %d)" % len(ROADS)

    ids = {a[0] for a in AREAS}
    seen = set()
    for i,(fr,to,d,b) in enumerate(ROADS):
        assert fr in ids and to in ids, "bad area in road %d" % i
        key = (min(fr,to), max(fr,to))
        assert key not in seen, "duplicate road %s" % (key,)
        seen.add(key)
        assert d > 0

    with open(os.path.join(data,"roads_db.csv"),"w",newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["id","from_area_id","to_area_id","distance_km","blocked"])
        for i,(fr,to,d,b) in enumerate(ROADS): w.writerow([i,fr,to,d,b])

    with open(os.path.join(data,"area_layout.csv"),"w",newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["id","x","y"])
        for i,name in AREAS:
            x,y = LAYOUT[i]; w.writerow([i,x,y])

    with open(os.path.join(data,"volunteers_db.csv"),"w",newline="") as f:
        w = csv.writer(f, lineterminator="\n"); w.writerow(["id","name","area_id","service","capacity","available"])
        for v in VOLUNTEERS: w.writerow(v)

    with open(os.path.join(data,"requests_db.csv"),"w",newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(["id","service","area_id","people_affected","people_covered",
                    "assigned_volunteers","status","message","owner_username"])
        for r in REQUESTS:
            w.writerow([r[0],r[1],r[2],r[3],r[4],";".join(str(x) for x in r[5]),r[6],r[7],r[8]])

    # consistency checks between seeded volunteers / requests / availability
    busy = set()
    for r in REQUESTS:
        if r[6] == "Active": busy |= set(r[5])
    for v in VOLUNTEERS:
        expected_unavailable = v[0] in busy
        assert (v[5] == 0) == expected_unavailable, "availability mismatch for vol %s" % v[1]
        cap = sum(x[4] for x in VOLUNTEERS if x[0] in set())
    # coverage checks
    caps = {v[0]: v[4] for v in VOLUNTEERS}
    for r in REQUESTS:
        total = sum(caps[i] for i in r[5])
        assert r[4] <= total, "covered exceeds capacity for request %d" % r[0]
        assert r[4] <= r[3], "covered exceeds affected for request %d" % r[0]

    print("seed data written: %d areas, %d roads, %d volunteers, %d requests"
          % (len(AREAS), len(ROADS), len(VOLUNTEERS), len(REQUESTS)))

if __name__ == "__main__":
    sys.exit(main())
