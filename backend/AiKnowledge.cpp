// =====================================================================
// AiKnowledge.cpp - the playbooks (see AiKnowledge.h)
//
// Guidance here is deliberately practical and conservative: it names the
// checks and crews a municipal department would normally line up, and it only
// tells a citizen to do things that are safe for a layperson. It never quotes
// legal sections or local helpline numbers (those differ by city). The only
// numbers it mentions are India's national emergency numbers (112 / 101).
// =====================================================================
#include "AiKnowledge.h"

namespace scms {
namespace ai {

namespace {

Playbook make(const char* id, const char* category, const char* title, int impact, int urgency, bool emergency) {
    Playbook p;
    p.id = id;
    p.category = category;
    p.title = title;
    p.defaultImpact = impact;
    p.defaultUrgency = urgency;
    p.emergency = emergency;
    return p;
}

Playbook makeGeneral(const char* id, const char* category, const char* title) {
    Playbook p = make(id, category, title, 3, 3, false);
    p.general = true;
    return p;
}

void addTraffic(std::vector<Playbook>& v) {
    {
        Playbook p = make("TRAFFIC_SIGNAL", "TRAFFIC", "Traffic signal not working", 4, 4, false);
        p.keywords = {"!signal", "signals", "traffic light", "traffic lights", "red light", "green light", "blinking", "signal timer",
                      "controller", "junction", "intersection"};
        p.explanation = "A signal outage turns a junction into an uncontrolled crossing, which is when most junction collisions happen.";
        p.causes = {"Power or UPS/battery failure at the signal controller", "Damaged cable or loop detector",
                    "Faulty LED module or controller timing plan"};
        p.immediate = {"Post a traffic warden or constable at the junction for manual control within 30 minutes.",
                       "Place cones or barricades to slow approaching vehicles while the signal is down."};
        p.fieldWork = {"Send the signal-maintenance crew to check mains supply, UPS/battery, controller and cabling.",
                       "Replace the failed LED module, fuse or controller card; restore the programmed timing plan.",
                       "Re-synchronise with the neighbouring signals so the corridor does not back up."};
        p.verify = {"Watch at least one full signal cycle on every arm of the junction.",
                    "Confirm pedestrian phases and timers work."};
        p.prevention = {"Add the junction to the preventive-maintenance round (battery, cabling, controller).",
                        "If it has failed more than twice in a quarter, plan a controller replacement."};
        p.resources = {"Traffic wardens with cones and barricades", "Signal maintenance van with spare LED modules and fuses"};
        p.citizenSafety = {"Treat the junction as a four-way stop: slow down and give way to the vehicle on your right/left as per local rules.",
                           "Cross with extra care and make eye contact with drivers before stepping out."};
        p.citizenTips = {"Say which junction and which arm of the signal is dark or stuck.",
                         "Mention the time of day - peak-hour outages get a warden faster."};
        p.resolvedNote = "The traffic signal has been repaired and tested through a full cycle.";
        v.push_back(p);
    }
    {
        Playbook p = make("TRAFFIC_PARKING", "TRAFFIC", "Illegal parking or obstruction", 3, 3, false);
        p.keywords = {"parked", "parking", "double parked", "illegal parking", "no parking", "blocking", "blocked road", "obstruct*",
                      "encroach*", "hawker", "vendor", "tow", "wrong parking", "footpath parking"};
        p.explanation = "Vehicles or stalls on the carriageway cut road capacity and force pedestrians onto the road.";
        p.causes = {"No marked or enforced parking bays nearby", "Repeat offenders at the same spot and time",
                    "Commercial loading with no designated loading window"};
        p.immediate = {"Send a patrol to the location, preferably at the time the complaint says it happens.",
                       "Tow away or issue notices to vehicles that block traffic or emergency access."};
        p.fieldWork = {"Issue challans as per the local traffic rules and record the vehicle numbers for repeat offenders.",
                       "Put up no-parking signage and mark the kerb so the restriction is clear.",
                       "Coordinate with the municipal team if the obstruction is a stall or construction material."};
        p.verify = {"Re-visit at the same time of day on the next two days to confirm the road stays clear."};
        p.prevention = {"Identify an alternative parking area or loading window for the shops nearby.",
                        "Schedule regular enforcement rounds for the time band in the complaint."};
        p.resources = {"Patrol vehicle and towing van", "Challan device and signage"};
        p.citizenSafety = {"Do not confront drivers; report the vehicle number and time instead."};
        p.citizenTips = {"Add vehicle numbers and the usual time of day if you can see them safely.",
                         "Say whether an ambulance or fire engine would be blocked."};
        p.resolvedNote = "Enforcement was carried out and the obstruction has been cleared.";
        v.push_back(p);
    }
    {
        Playbook p = make("TRAFFIC_CONGESTION", "TRAFFIC", "Congestion or traffic bottleneck", 4, 3, false);
        p.keywords = {"jam*", "congestion", "congested", "bottleneck", "peak hour", "peak hours", "traffic jam", "school gate",
                      "warden", "slow moving", "crawl*", "gridlock", "rush hour", "diversion"};
        p.explanation = "A recurring jam usually points to a signal-timing, parking or turning-movement problem, not just 'too many cars'.";
        p.causes = {"Signal phases not matched to peak flows", "Pick-up/drop-off at a school or market gate spilling onto the road",
                    "Narrow lane, unmarked turn or on-street parking"};
        p.immediate = {"Deploy wardens at the choke point during the reported hours.",
                       "Check whether a signal failure or road work is the trigger today."};
        p.fieldWork = {"Run a short peak-hour count and adjust signal phasing or add a green-extension.",
                       "Introduce a one-way flow, a turn restriction or a diversion where it helps.",
                       "Work with the school or market association on staggered timings and a pick-up bay."};
        p.verify = {"Compare queue length before and after at the same hour for three days."};
        p.prevention = {"Add the spot to the monthly traffic review.", "Consider signage and road markings that guide lane discipline."};
        p.resources = {"Traffic wardens for peak hours", "Signal engineer", "Barricades and signage"};
        p.citizenSafety = {"If you must travel through it, avoid the exact peak window or use the nearest alternative road."};
        p.citizenTips = {"Say the hours when it is worst and which direction backs up.",
                         "Mention nearby schools, markets or events that explain the load."};
        p.resolvedNote = "Traffic management measures are in place at this spot; please tell us if the jam returns.";
        v.push_back(p);
    }
    {
        Playbook p = make("TRAFFIC_SAFETY", "TRAFFIC", "Road-safety hazard or accident-prone spot", 4, 4, true);
        p.keywords = {"accident", "accidents", "rash", "speeding", "overspeed*", "zebra", "pedestrian crossing", "no crossing", "blind spot",
                      "black spot", "wrong side", "drunk", "helmet", "collision", "unsafe crossing", "near miss", "crash*"};
        p.explanation = "Repeated accidents or near-misses at one place mean the road layout or enforcement needs a safety fix.";
        p.causes = {"Missing crossing, speed breaker or signage", "Poor visibility or lighting", "Speeding with no enforcement"};
        p.immediate = {"Send a patrol and place temporary warning signs or barricades if there is an immediate danger.",
                       "Check the location against recent accident reports."};
        p.fieldWork = {"Install or repaint zebra crossings, add rumble strips or speed breakers where appropriate.",
                       "Fix signage and reflective markers; request better street lighting from the power department.",
                       "Schedule speed-enforcement drives for the stretch."};
        p.verify = {"Do a night-time inspection to confirm markings and signs are visible."};
        p.prevention = {"Add the spot to the accident-prone list for quarterly review.",
                        "Share the finding with the roads department if the layout needs engineering changes."};
        p.resources = {"Patrol team", "Road-marking and signage crew", "Coordination with PWD and street-lighting teams"};
        p.citizenSafety = {"Avoid the spot after dark if possible and cross only where traffic can see you.",
                           "If someone is injured, call 112 immediately."};
        p.citizenTips = {"Describe exactly what makes it dangerous (speed, no crossing, blind turn).",
                         "Mention any accident you know of, with approximate dates."};
        p.resolvedNote = "Road-safety measures have been installed at this location.";
        v.push_back(p);
    }
    {
        Playbook p = makeGeneral("TRAFFIC_GENERAL", "TRAFFIC", "General traffic issue");
        p.explanation = "A traffic-related problem that does not match a specific pattern yet.";
        p.causes = {"Signal, parking or road-layout problem - the field visit will decide"};
        p.immediate = {"Send a traffic warden to inspect the location and report what is causing the problem."};
        p.fieldWork = {"Act on the inspection finding (signal fix, enforcement, signage or diversion)."};
        p.verify = {"Re-visit at the same hour on the next day to confirm the problem is gone."};
        p.prevention = {"Record the finding so repeat complaints from this spot can be linked."};
        p.resources = {"Traffic warden", "Signage and barricades"};
        p.citizenSafety = {"Stay on footpaths and cross only at safe points until it is fixed."};
        p.citizenTips = {"Add the time of day and the exact spot to help the team find it."};
        p.resolvedNote = "The traffic issue at this location has been addressed.";
        v.push_back(p);
    }
}

void addGarbage(std::vector<Playbook>& v) {
    {
        Playbook p = make("GARBAGE_MISSED", "GARBAGE", "Missed garbage collection", 3, 3, false);
        p.keywords = {"not collected", "not picked", "not been collected", "collection", "pickup", "pick up", "door to door",
                      "no one comes", "vehicle not coming", "garbage van", "missed", "irregular", "not cleared"};
        p.explanation = "A missed round is usually a vehicle, staffing or route problem rather than a one-off.";
        p.causes = {"Collection vehicle breakdown or route change", "Staff shortage on the ward", "Lane skipped because of access or parked vehicles"};
        p.immediate = {"Check the route log or vehicle tracking for the lane and confirm what was skipped.",
                       "Dispatch a backup vehicle the same day to clear the accumulated waste."};
        p.fieldWork = {"Brief the ward sanitary inspector and restore the normal collection timing.",
                       "Sprinkle disinfectant or lime at the spot where waste had piled up.",
                       "Tell residents the revised collection window."};
        p.verify = {"Confirm the next two scheduled collections happen on time."};
        p.prevention = {"Keep a standby vehicle and crew for the ward.", "Add the lane to the supervisor's daily check."};
        p.resources = {"Backup collection vehicle and crew", "Sanitary inspector", "Disinfectant or lime"};
        p.citizenSafety = {"Keep waste bagged and covered inside your premises until the vehicle comes.",
                           "Do not burn garbage - the smoke is harmful and it is not allowed."};
        p.citizenTips = {"Say how many days collection has been missed and the lane or street name.",
                         "Tell us the usual collection time so the supervisor can check it."};
        p.resolvedNote = "The accumulated waste has been collected and regular pickup has resumed.";
        v.push_back(p);
    }
    {
        Playbook p = make("GARBAGE_OVERFLOW", "GARBAGE", "Overflowing bin or garbage dump", 4, 3, false);
        p.keywords = {"overflow*", "bin", "dumpster", "dump", "stink*", "smell", "stray", "dogs", "flies", "rats", "pile",
                      "heap", "community bin", "dumping ground"};
        p.explanation = "An overflowing bin or dump is a public-health risk: it attracts strays, rats and flies and spreads odour.";
        p.causes = {"Bin lifted less often than waste is generated", "Bin too small for the catchment", "No lid, so animals scatter the waste"};
        p.immediate = {"Lift the waste and clean the spot the same day.", "Disinfect the area with bleaching powder or lime."};
        p.fieldWork = {"Increase lifting frequency or add a second bin for the catchment.",
                       "Fix or replace damaged bins and add lids.",
                       "Check whether commercial waste is being dumped into the community bin."};
        p.verify = {"Inspect the spot after the next scheduled lift and again a day later."};
        p.prevention = {"Re-plan the bin location and capacity.", "Encourage door-to-door collection for the nearby lanes."};
        p.resources = {"Compactor or tipper", "Cleaning crew", "Disinfectant, replacement bins with lids"};
        p.citizenSafety = {"Keep children and pets away from the dump.", "Wash hands after contact with anything from the area."};
        p.citizenTips = {"Describe how often it overflows and whether it is worse on certain days.",
                         "Mention any health concern such as rats, flies or mosquitoes."};
        p.resolvedNote = "The overflowing waste has been cleared and the spot disinfected.";
        v.push_back(p);
    }
    {
        Playbook p = make("GARBAGE_DEBRIS", "GARBAGE", "Illegal dumping, debris or waste burning", 3, 3, false);
        p.keywords = {"debris", "construction", "rubble", "dumped", "dumping", "illegal dump*", "burning", "burn", "burnt", "smoke",
                      "demolition", "building material", "sand", "bricks", "footpath blocked"};
        p.explanation = "Construction debris or dumped waste blocks footpaths and drains; burning waste adds a smoke and fire risk.";
        p.causes = {"Contractor or household dumping without a collection arrangement", "No bin or debris-collection point nearby"};
        p.immediate = {"Inspect the site and identify who is responsible if possible.",
                       "If waste is burning, inform the fire service and have the fire put out first."};
        p.fieldWork = {"Remove the debris with a loader and tipper and dispose of it at the designated site.",
                       "Serve a notice or penalty on the responsible party as per local rules.",
                       "Put up a 'no dumping' board and consider a camera at repeat spots."};
        p.verify = {"Re-visit after two days to confirm no fresh dumping."};
        p.prevention = {"Set up a registered debris-collection service for the ward.", "Add the spot to the night-patrol list."};
        p.resources = {"Loader and tipper truck", "Enforcement inspector", "Signage"};
        p.citizenSafety = {"Stay away from burning waste - the smoke can be toxic.", "Call 101 if the fire is spreading."};
        p.citizenTips = {"Note vehicle numbers or the time of dumping if you can see them safely.",
                         "Say how much of the footpath or road is blocked."};
        p.resolvedNote = "The debris has been removed and the site is being monitored.";
        v.push_back(p);
    }
    {
        Playbook p = make("GARBAGE_HAZARD", "GARBAGE", "Dead animal or hazardous waste", 4, 5, true);
        p.keywords = {"!dead animal", "!carcass", "dead dog", "dead cow", "dead cat", "biomedical", "bio medical", "hospital waste",
                      "syringe*", "needle*", "medical waste", "chemical", "hazardous", "sewage waste", "toxic"};
        p.explanation = "Carcasses and medical or chemical waste need specially equipped teams, not the ordinary collection crew.";
        p.causes = {"Illegal disposal by a clinic or business", "Animal death on the roadside"};
        p.immediate = {"Send a trained team with gloves, masks and sealed containers - never the regular crew bare-handed.",
                       "Cordon off the spot while it is being cleared."};
        p.fieldWork = {"Remove and dispose of the carcass or hazardous waste through the authorised channel.",
                       "Disinfect the site thoroughly.",
                       "Inform the health department and trace the source of medical or chemical waste."};
        p.verify = {"Confirm the site is clean and odour-free and that nothing sharp remains."};
        p.prevention = {"Inspect nearby clinics and businesses for proper waste disposal."};
        p.resources = {"Hazardous-waste team with PPE", "Sealed containers", "Disinfectant"};
        p.citizenSafety = {"Do not touch needles, syringes, chemicals or carcasses.", "Keep children and pets away; call 112 if a child may have been hurt."};
        p.citizenTips = {"Say exactly what you can see and from what distance.", "Do not try to move it yourself."};
        p.resolvedNote = "The hazardous item has been removed safely and the area disinfected.";
        v.push_back(p);
    }
    {
        Playbook p = makeGeneral("GARBAGE_GENERAL", "GARBAGE", "General sanitation issue");
        p.explanation = "A cleanliness or waste problem that does not match a specific pattern yet.";
        p.causes = {"Collection gap, bin capacity or dumping - the inspection will decide"};
        p.immediate = {"Send the sanitary inspector to the spot to see what is wrong."};
        p.fieldWork = {"Clear the waste and fix the cause found during the inspection."};
        p.verify = {"Re-inspect after the next scheduled collection."};
        p.prevention = {"Add the lane to the supervisor's daily check."};
        p.resources = {"Cleaning crew and vehicle", "Disinfectant"};
        p.citizenSafety = {"Keep waste covered and away from children and pets."};
        p.citizenTips = {"Add the street name and how long the problem has existed."};
        p.resolvedNote = "The sanitation issue at this location has been addressed.";
        v.push_back(p);
    }
}

void addElectricity(std::vector<Playbook>& v) {
    {
        Playbook p = make("ELEC_EXPOSED", "ELECTRICITY", "Exposed, hanging or sparking wires", 5, 5, true);
        p.keywords = {"exposed", "hanging", "low hanging", "wire", "wires", "cable", "cables", "spark*", "shock", "electrocut*", "live wire",
                      "fallen pole", "broken pole", "leaning pole", "pole", "open box", "meter box", "short circuit", "snapped"};
        p.explanation = "Exposed or fallen conductors can kill. This is a life-safety job, not a routine repair.";
        p.causes = {"Storm or vehicle damage to a line or pole", "Ageing insulation or loose joints", "Open or damaged feeder pillar / meter box"};
        p.immediate = {"Isolate the affected feeder first (switch off and lock out) before anyone goes near the wire.",
                       "Cordon the area and post a person to keep the public away until the crew arrives.",
                       "Dispatch a lineman team with PPE - target first response within 1-2 hours."};
        p.fieldWork = {"Repair, re-tension or replace the conductor; insulate or replace the damaged joints.",
                       "Replace or straighten a damaged pole and secure stay wires.",
                       "Close and lock open boxes; check earthing at the point."};
        p.verify = {"Run an insulation and earthing check before re-energising.", "Inspect adjacent spans for the same fault."};
        p.prevention = {"Add the line to the tree-trimming and ageing-asset inspection list.",
                        "Review the area for other low-hanging or damaged lines."};
        p.resources = {"Lineman team with PPE and insulated tools", "Bucket truck or ladder", "Replacement conductor, insulators and pole hardware"};
        p.citizenSafety = {"Stay at least 10 metres away from a fallen or sparking wire and keep children and animals away.",
                           "Do not touch anything that is in contact with the wire (metal fences, puddles, vehicles).",
                           "Call 112 immediately if anyone has been shocked or there is a fire."};
        p.citizenTips = {"Give the exact pole number or nearest landmark if you can read it from a safe distance.",
                         "Say whether the wire is on the ground, over a road or near a school or park."};
        p.resolvedNote = "The hazardous wiring has been made safe and the line repaired.";
        v.push_back(p);
    }
    {
        Playbook p = make("ELEC_OUTAGE", "ELECTRICITY", "Power cut or supply interruption", 4, 4, false);
        p.keywords = {"power cut", "power cuts", "no power", "no electricity", "outage", "blackout", "load shedding", "light gone", "lights gone",
                      "power supply", "electricity supply", "tripping", "trips", "frequent cuts", "no light", "electricity gone", "power failure"};
        p.explanation = "Frequent or long outages normally come from a feeder fault, overloaded line or planned maintenance that was not announced.";
        p.causes = {"Feeder or substation trip", "Blown fuse or failed cable joint", "Overload at peak hours", "Planned maintenance"};
        p.immediate = {"Check the feeder and substation status and whether any planned shutdown covers the area.",
                       "Localise the fault and switch the area to an alternate feeder if one is available."};
        p.fieldWork = {"Repair the fault (fuse, joint, cable or breaker) and restore supply.",
                       "Give an estimated restoration time to the area and prioritise hospitals, water-pumping stations and schools.",
                       "Review the load on the feeder if cuts happen at the same hours."};
        p.verify = {"Confirm voltage is normal at several points in the area.", "Check that supply stays on for the next evening peak."};
        p.prevention = {"Balance the load or upgrade the feeder if it trips repeatedly.", "Announce planned shutdowns in advance."};
        p.resources = {"Fault-finding team", "Spare fuses, joints and cable", "Alternate-feeder switching plan"};
        p.citizenSafety = {"Switch off heavy appliances so they do not surge when power returns.",
                           "Use a torch rather than a candle and keep medical equipment on backup power."};
        p.citizenTips = {"Say when the cuts start and how long they last.", "Check whether the whole street is out or only your building."};
        p.resolvedNote = "Power supply has been restored and the fault repaired.";
        v.push_back(p);
    }
    {
        Playbook p = make("ELEC_TRANSFORMER", "ELECTRICITY", "Transformer fault or low voltage", 4, 4, true);
        p.keywords = {"!transformer", "blast", "burnt", "burned", "oil leak", "fuse", "dtc", "overload*", "low voltage",
                      "voltage", "dim", "fluctuat*", "humming", "buzzing", "explosion"};
        p.explanation = "Transformer faults affect a whole neighbourhood and a burning or leaking unit is also a fire risk.";
        p.causes = {"Overloaded transformer", "Oil leak or internal fault", "Loose connections or failed fuse"};
        p.immediate = {"Inspect the unit for smoke, oil leakage and damage; keep people away if it is burning or humming loudly.",
                       "Check the load on the transformer against its rating."};
        p.fieldWork = {"Replace the fuse or repair the connection if that is the cause.",
                       "Swap in a spare transformer or a mobile unit if the unit has failed.",
                       "Balance load across phases or add capacity if overload is the cause."};
        p.verify = {"Measure voltage at the far end of the lines after the fix.", "Observe for temperature or noise after energising."};
        p.prevention = {"Include the unit in the preventive-maintenance list (oil level, fuses, earthing).",
                        "Plan a capacity upgrade if the area has grown."};
        p.resources = {"Transformer maintenance crew", "Spare transformer or mobile unit", "Fire extinguisher"};
        p.citizenSafety = {"Stay away from the transformer and keep children, animals and vehicles back.",
                           "Unplug sensitive electronics while the voltage is unstable.", "Call 101 if there is fire or heavy smoke."};
        p.citizenTips = {"Describe what you saw or heard: smoke, sparks, a bang or a hum.",
                         "Say how many houses see dim lights or low voltage."};
        p.resolvedNote = "The transformer issue has been fixed and voltage is back to normal.";
        v.push_back(p);
    }
    {
        Playbook p = make("ELEC_STREETLIGHT", "ELECTRICITY", "Street light not working", 2, 3, false);
        p.keywords = {"street light", "street lights", "streetlight", "streetlights", "lamp", "lamps", "dark", "darkness", "light not working",
                      "pole light", "no light on road", "bulb", "led light"};
        p.explanation = "Dark road stretches raise accident and crime risk, so clusters and junctions come first.";
        p.causes = {"Failed lamp, driver or ballast", "Faulty photocell or timer", "Cable fault or tripped control panel"};
        p.immediate = {"Check whether one lamp or a whole cluster is out - a cluster usually means a panel or cable fault.",
                       "Note the pole number for the work order."};
        p.fieldWork = {"Replace the lamp, driver or photocell; repair the cable or reset the control panel.",
                       "Give priority to junctions, crossings and stretches with a record of accidents."};
        p.verify = {"Do a night inspection to confirm the light comes on and stays on."};
        p.prevention = {"Include the road in the periodic night-patrol and lamp-replacement schedule."};
        p.resources = {"Street-light maintenance van with ladder or lift", "Spare LED lamps and drivers"};
        p.citizenSafety = {"Avoid walking the dark stretch alone at night; use a torch and stay on the footpath."};
        p.citizenTips = {"Give the pole number printed on the pole if you can see it.", "Say how many lights in a row are out."};
        p.resolvedNote = "The street light has been repaired and checked at night.";
        v.push_back(p);
    }
    {
        Playbook p = makeGeneral("ELEC_GENERAL", "ELECTRICITY", "General electricity issue");
        p.explanation = "An electricity problem that does not match a specific pattern yet. Treat any exposed wiring as dangerous.";
        p.causes = {"Fault on the supply line, meter or street lighting - the inspection will decide"};
        p.immediate = {"Send a lineman team to inspect; isolate first if anything is exposed or sparking."};
        p.fieldWork = {"Repair the fault found during the inspection and restore normal supply."};
        p.verify = {"Test voltage and insulation before closing the job."};
        p.prevention = {"Log the pole or feeder number for repeat-fault analysis."};
        p.resources = {"Lineman team with PPE", "Spare fuses and cable"};
        p.citizenSafety = {"Never touch exposed wiring and keep others away; call 112 if there is any danger."};
        p.citizenTips = {"Say whether it affects one house, a street or a whole area."};
        p.resolvedNote = "The electricity issue at this location has been fixed.";
        v.push_back(p);
    }
}

void addWater(std::vector<Playbook>& v) {
    {
        Playbook p = make("WATER_NO_SUPPLY", "WATER", "No or low water supply", 4, 4, false);
        p.keywords = {"no water", "no supply", "water supply", "dry", "dry taps", "low pressure", "tanker",
                      "not coming", "taps", "tap", "shortage", "no water supply"};
        p.explanation = "A supply failure is normally caused by pumping, valve or pipeline trouble upstream, not the household.";
        p.causes = {"Power failure at the pumping station", "Closed or faulty valve", "Pipeline burst or heavy leakage upstream",
                    "Low reservoir level or unscheduled valve operation"};
        p.immediate = {"Check the pumping station, reservoir level and valve schedule for the zone.",
                       "Arrange water tankers for the affected area, starting with hospitals, schools and elderly residents."};
        p.fieldWork = {"Locate and fix the cause (restart pumps, open the correct valve, repair a leak or burst).",
                       "Publish the supply timetable and the expected restoration time.",
                       "Check for illegal connections or leakage that drain pressure away from the area."};
        p.verify = {"Measure pressure at the tail-end connections.", "Confirm supply reaches the complaining lane in the next cycle."};
        p.prevention = {"Install a standby pump and valve-position monitoring.", "Review the supply schedule for the zone."};
        p.resources = {"Pump and valve crew", "Water tankers", "Pressure gauges"};
        p.citizenSafety = {"Store water in clean, covered containers and use it sparingly until supply returns.",
                           "Make sure only treated water is used for drinking."};
        p.citizenTips = {"Check whether your whole building or only your tap is dry (your own tank, motor or valve may be the cause).",
                         "Ask a neighbour on the next street - it helps the team find the faulty section."};
        p.resolvedNote = "Water supply has been restored and pressure checked at the affected connections.";
        v.push_back(p);
    }
    {
        Playbook p = make("WATER_CONTAMINATION", "WATER", "Dirty, smelly or contaminated water", 5, 5, false);
        p.keywords = {"dirty", "muddy", "smell", "foul", "contaminat*", "colour", "color", "yellow", "brown", "black water", "worms",
                      "taste", "diarrh*", "illness", "sick", "vomit*", "cholera", "typhoid", "jaundice", "unsafe water", "polluted"};
        p.explanation = "Discoloured or foul-smelling tap water can mean sewage ingress or pipe damage and is a direct health risk.";
        p.causes = {"Sewage or drain water entering a leaking pipe", "Sediment stirred up after a burst or repair", "Inadequate chlorination"};
        p.immediate = {"Send a team to collect water samples and test chlorine residual and bacteria.",
                       "Issue a boil-water advisory for the affected lanes and arrange safe water by tanker.",
                       "Inform the health department if people report illness."};
        p.fieldWork = {"Trace the contamination source: check for pipe leaks near drains and cross-connections with sewers.",
                       "Flush the line and re-chlorinate once the leak is repaired.",
                       "Repair or replace the damaged pipe section."};
        p.verify = {"Confirm test results are clean and chlorine residual is within limits before lifting the advisory."};
        p.prevention = {"Schedule routine sampling in the zone.", "Inspect old pipelines that run next to drains."};
        p.resources = {"Water-quality testing kit", "Chlorination team", "Pipe repair crew", "Safe-water tankers"};
        p.citizenSafety = {"Do not drink the water untreated: boil it at a rolling boil for at least one minute and let it cool, or use sealed bottled water.",
                           "Use safe water for infants and formula, and for washing fruit and vegetables.",
                           "See a doctor if anyone has stomach illness; this is not a diagnosis, just a caution."};
        p.citizenTips = {"Describe the colour, smell and when it started; a clear glass photo kept for yourself helps explain it to the team.",
                         "Say how many households see the same problem."};
        p.resolvedNote = "The source of contamination was fixed and the water has been tested and is safe.";
        v.push_back(p);
    }
    {
        Playbook p = make("WATER_LEAK", "WATER", "Pipeline leak or burst", 4, 4, false);
        p.keywords = {"leak", "leaks", "leaking", "burst", "pipe", "pipes", "pipeline", "wasting", "gushing", "overflowing tank", "water on road",
                      "flooded road", "puddle", "seepage", "joint"};
        p.explanation = "Leaks waste treated water, damage the road underneath and let contaminants into the line.";
        p.causes = {"Corroded pipe or failed joint", "Damage from digging or heavy vehicles", "Pressure surges"};
        p.immediate = {"Close the nearest valves to isolate the leaking section and stop the loss.",
                       "Warn downstream consumers about the interruption."};
        p.fieldWork = {"Excavate, then repair with a clamp or replace the damaged pipe section.",
                       "Flush and chlorinate the line after the repair before restoring supply.",
                       "Coordinate with the roads department to restore the cut road surface."};
        p.verify = {"Check for a pressure drop or fresh wet patches for 24 hours after the repair."};
        p.prevention = {"Flag old pipe sections for replacement.", "Record the leak location so repeat failures stand out."};
        p.resources = {"Pipe-repair crew with clamps and pipes", "Excavation equipment", "Barricades and signage"};
        p.citizenSafety = {"Keep away from the flooded area and do not drive through deep or fast-moving water.",
                           "Do not operate public valves yourself."};
        p.citizenTips = {"Give the exact spot and say how strong the flow is.", "Mention whether the water looks clean or dirty."};
        p.resolvedNote = "The leaking pipe has been repaired and supply restored.";
        v.push_back(p);
    }
    {
        Playbook p = make("WATER_SEWER", "WATER", "Sewer overflow or blockage", 5, 4, false);
        p.keywords = {"!sewer", "sewers", "!sewage", "septic", "manhole overflow", "sewerage", "toilet overflow", "overflowing sewer", "choke", "choked sewer",
                      "gutter", "stench", "faeces", "latrine"};
        p.explanation = "Sewage on the street is an immediate health hazard and usually means a blocked or overloaded sewer line.";
        p.causes = {"Blockage from grease, plastic or silt", "Pump or lift-station failure", "Collapsed or undersized line"};
        p.immediate = {"Send a sewer-cleaning crew with a jetting or suction machine.",
                       "Barricade the area and keep people away from the overflow."};
        p.fieldWork = {"Clear the choke and flush the line.", "Disinfect the affected area with lime or bleaching powder.",
                       "Repair or replace the section if it has collapsed; restart the pump if it failed."};
        p.verify = {"Confirm the sewer flows freely and that there is no overflow at the next manhole."};
        p.prevention = {"Add the line to a regular desilting round.", "Run a campaign against dumping grease and plastic into drains."};
        p.resources = {"Jetting / suction machine", "Sewer crew with PPE", "Disinfectant"};
        p.citizenSafety = {"Avoid contact with the water; keep children and pets away.", "Wash hands and footwear if you came into contact."};
        p.citizenTips = {"Say which manholes overflow and how long it has lasted.", "Do not pour grease or put plastic down drains."};
        p.resolvedNote = "The sewer has been cleared and the area disinfected.";
        v.push_back(p);
    }
    {
        Playbook p = makeGeneral("WATER_GENERAL", "WATER", "General water or sewerage issue");
        p.explanation = "A water or sewerage problem that does not match a specific pattern yet.";
        p.causes = {"Supply, quality or pipeline fault - the field inspection will decide"};
        p.immediate = {"Send a team to inspect the location and check the nearby valves and pipeline."};
        p.fieldWork = {"Fix the cause found during the inspection and restore normal service."};
        p.verify = {"Check pressure and water quality after the fix."};
        p.prevention = {"Log the pipeline segment for repeat-fault analysis."};
        p.resources = {"Water-supply crew", "Pressure gauge"};
        p.citizenSafety = {"Use only boiled or bottled water for drinking if the water looks or smells unusual."};
        p.citizenTips = {"Say whether it affects only you or the whole street."};
        p.resolvedNote = "The water issue at this location has been addressed.";
        v.push_back(p);
    }
}

void addRoads(std::vector<Playbook>& v) {
    {
        Playbook p = make("ROADS_POTHOLE", "ROADS", "Potholes", 4, 4, false);
        p.keywords = {"!pothole", "potholes", "pit", "pits", "crater", "craters", "hole in road", "holes", "two wheeler", "two-wheeler", "bike skid",
                      "accident", "tyre", "waterfilled", "water filled"};
        p.explanation = "Potholes cause skids and falls, especially for two-wheelers, and grow quickly once water gets under the surface.";
        p.causes = {"Water seeping into a cracked surface", "Heavy traffic on a worn surface", "Poor repair after a utility cut"};
        p.immediate = {"Mark or barricade the pothole so riders can see it, especially on main roads and at night.",
                       "Log the exact location and size for the work order."};
        p.fieldWork = {"Cut the edges square, clean out the pothole and fill it in layers with the right mix (cold mix is acceptable in wet weather).",
                       "Compact the patch and level it with the road surface; add reflective markers if the road is dark.",
                       "Check for a drainage problem that keeps undermining the road."};
        p.verify = {"Check that the patch is flush with the surface and well compacted.", "Re-check after the next heavy rain."};
        p.prevention = {"If several potholes are on one stretch, plan resurfacing instead of patching.", "Fix drains next to the road edge."};
        p.resources = {"Patching gang with cold-mix or hot-mix asphalt", "Compactor", "Barricades and reflective markers"};
        p.citizenSafety = {"Slow down on that stretch, especially on two-wheelers and at night.",
                           "Do not swerve suddenly to avoid it; brake early instead."};
        p.citizenTips = {"Say how big and how deep the pothole is and which lane it is in.", "Mention any accident or near-miss you know of there."};
        p.resolvedNote = "The potholes have been filled, compacted and levelled.";
        v.push_back(p);
    }
    {
        Playbook p = make("ROADS_DAMAGE", "ROADS", "Damaged or washed-away road surface", 4, 3, false);
        p.keywords = {"damaged road", "broken road", "washed", "washed away", "cracks", "cracked", "caved", "caved in", "sinking", "sunk", "subsidence",
                      "resurfac*", "uneven", "rough road", "bad road", "road cut", "collapsed road", "eroded"};
        p.explanation = "A damaged surface is often a drainage or base-layer failure; a quick patch alone usually does not last.";
        p.causes = {"Heavy rain and poor drainage", "Weak base layer", "Utility trench not properly restored"};
        p.immediate = {"Inspect the stretch; if it is caving in or sinking, barricade it and divert traffic until an engineer clears it.",
                       "Record the extent (length and width) with photos for the work order."};
        p.fieldWork = {"Do temporary stabilisation (gravel or WBM) so the road is usable.",
                       "Plan permanent repair or resurfacing and fix the drainage or base problem causing the damage.",
                       "Coordinate with the utility that dug the trench if the damage comes from a road cut."};
        p.verify = {"Test with a loaded vehicle pass and check there is no settling."};
        p.prevention = {"Include the stretch in the next resurfacing programme.", "Inspect after each heavy monsoon spell."};
        p.resources = {"Roads maintenance gang", "Roller and paver", "Barricades and diversion signs"};
        p.citizenSafety = {"Avoid the damaged stretch if you can and keep to the stable side of the road.",
                           "Keep children away from road edges that are crumbling or sinking."};
        p.citizenTips = {"Say how long the damaged stretch is and whether it is getting worse.", "Mention if water collects there after rain."};
        p.resolvedNote = "The damaged road section has been repaired.";
        v.push_back(p);
    }
    {
        Playbook p = make("ROADS_FOOTPATH", "ROADS", "Broken footpath or pavement", 3, 3, false);
        p.keywords = {"footpath", "footpaths", "pavement", "pavements", "tiles", "walkway", "sidewalk", "broken tiles", "uneven footpath",
                      "senior", "wheelchair"};
        p.explanation = "A broken footpath pushes pedestrians, including seniors and wheelchair users, onto the carriageway.";
        p.causes = {"Broken or lifted tiles", "Tree roots or utility digging", "Encroachment"};
        p.immediate = {"Inspect and cone off any section where someone could trip or fall.",
                       "Check whether an encroachment or utility cover is the cause."};
        p.fieldWork = {"Replace or re-lay broken tiles and level the surface.",
                       "Fit kerb ramps so wheelchairs and prams can use the footpath.",
                       "Coordinate with the municipal team to clear any encroachment."};
        p.verify = {"Walk the full stretch to check there are no gaps or sharp edges."};
        p.prevention = {"Add the road to the footpath audit list.", "Ensure utility digging is restored properly."};
        p.resources = {"Paving crew", "Tiles, sand and cement", "Cones"};
        p.citizenSafety = {"Walk carefully, especially in the dark, and use the safest side of the road if the footpath is blocked."};
        p.citizenTips = {"Say who is most affected (seniors, schoolchildren, wheelchair users).", "Give the start and end of the damaged section."};
        p.resolvedNote = "The footpath has been repaired and levelled.";
        v.push_back(p);
    }
    {
        Playbook p = make("ROADS_STRUCTURE", "ROADS", "Bridge, culvert, divider or railing damage", 5, 5, true);
        p.keywords = {"!bridge", "!culvert", "divider", "railing", "railings", "median", "speed breaker", "signage", "crash barrier", "guard rail",
                      "flyover", "underpass", "retaining wall", "parapet", "cracked bridge"};
        p.explanation = "Damage to structures is a potential collapse or fall risk and needs an engineer, not just a repair crew.";
        p.causes = {"Vehicle impact", "Corrosion or ageing", "Flood or scour damage"};
        p.immediate = {"Arrange a structural engineer's inspection the same day.",
                       "Barricade the damaged part and restrict traffic or load until it is declared safe."};
        p.fieldWork = {"Carry out the repair as the engineer specifies; replace damaged railing, dividers or crash barriers.",
                       "Place warning signs and reflective markers around the repair."};
        p.verify = {"Get the engineer's sign-off before reopening the structure to full traffic."};
        p.prevention = {"Add the structure to the periodic inspection schedule."};
        p.resources = {"Structural engineer", "Repair gang", "Barricades and warning signs"};
        p.citizenSafety = {"Do not cross or park on a structure that looks cracked or sagging.",
                           "Call 112 if there is an immediate danger to people."};
        p.citizenTips = {"Describe what is damaged and any movement or cracking you can see from a safe distance."};
        p.resolvedNote = "The structure has been inspected and repaired as per the engineer's advice.";
        v.push_back(p);
    }
    {
        Playbook p = makeGeneral("ROADS_GENERAL", "ROADS", "General road issue");
        p.explanation = "A road problem that does not match a specific pattern yet.";
        p.causes = {"Surface wear, drainage or structural cause - the inspection will decide"};
        p.immediate = {"Send a roads inspector to measure and photograph the problem."};
        p.fieldWork = {"Repair as per the inspection, with temporary barricading if there is a risk."};
        p.verify = {"Check the finished surface is level and sound."};
        p.prevention = {"Log the stretch for the next maintenance round."};
        p.resources = {"Roads maintenance gang", "Barricades"};
        p.citizenSafety = {"Slow down on the stretch and keep clear of the damaged part."};
        p.citizenTips = {"Say how large the damaged area is and whether it is getting worse."};
        p.resolvedNote = "The road issue at this location has been repaired.";
        v.push_back(p);
    }
}

void addDrainage(std::vector<Playbook>& v) {
    {
        Playbook p = make("DRAIN_BLOCKED", "DRAINAGE", "Blocked or choked drain", 3, 3, false);
        p.keywords = {"blocked", "choked", "choke", "clogged", "clog", "plastic", "silt", "silted", "not flowing", "stagnant drain",
                      "drain not cleaned", "desilt*", "garbage in drain"};
        p.explanation = "A choked drain is the usual reason water backs up after rain and a ready breeding place for mosquitoes.";
        p.causes = {"Plastic and silt build-up", "Waste thrown into the drain", "Collapsed or crushed section"};
        p.immediate = {"Inspect the drain and the next inlet or outfall to find where the flow stops."};
        p.fieldWork = {"Desilt the drain with a jetting or manual crew and remove plastic and waste.",
                       "Take the silt away immediately - do not leave it beside the drain.",
                       "Repair any broken or collapsed section."};
        p.verify = {"Pour or observe water to confirm free flow from the inlet to the outfall."};
        p.prevention = {"Add the drain to the pre-monsoon desilting list.", "Fit trash screens at inlets where plastic collects."};
        p.resources = {"Desilting crew with a jetting machine", "Tipper for silt", "Trash screens"};
        p.citizenSafety = {"Keep away from the open drain and keep children and pets from playing near it."};
        p.citizenTips = {"Do not dump waste in or beside the drain.", "Say which stretch is blocked and since when."};
        p.resolvedNote = "The drain has been desilted and is flowing freely.";
        v.push_back(p);
    }
    {
        Playbook p = make("DRAIN_WATERLOG", "DRAINAGE", "Waterlogging or flooding", 4, 4, false);
        p.keywords = {"waterlogging", "waterlogged", "water logging", "flood", "flooding", "flooded", "stagnant", "knee deep", "knee-deep",
                      "submerged", "rain", "monsoon", "inundat*", "standing water", "mosquito", "mosquitoes", "dengue", "malaria"};
        p.explanation = "Standing water after rain points to choked inlets, low-lying ground or an overloaded outfall, and breeds mosquitoes within days.";
        p.causes = {"Choked inlets or gully pits", "Outfall blocked or at high water", "Drain capacity too small for the catchment"};
        p.immediate = {"Deploy dewatering pumps to the low point and clear the nearest inlets.",
                       "Coordinate with traffic police for a diversion if the road is flooded."};
        p.fieldWork = {"Open and desilt gully pits and clear the outfall.",
                       "After the water recedes, ask the health team for larvicide spraying and fogging.",
                       "Identify low spots that need a permanent drain or raised road level."};
        p.verify = {"Check the area drains within a few hours of the next rain."};
        p.prevention = {"Add the spot to the monsoon-readiness list with a standby pump.", "Review drain size for the catchment."};
        p.resources = {"Dewatering pumps and hoses", "Desilting crew", "Barricades, coordination with traffic police and health team"};
        p.citizenSafety = {"Do not walk or drive through floodwater: it can hide open manholes and electric hazards.",
                           "Keep children away from the water and switch off the mains if water enters your home."};
        p.citizenTips = {"Say how deep the water gets and how long it stays after rain.", "Mention mosquito breeding or a health concern."};
        p.resolvedNote = "The waterlogging has been cleared and the drains opened.";
        v.push_back(p);
    }
    {
        Playbook p = make("DRAIN_OPEN", "DRAINAGE", "Open manhole, missing cover or overflowing drain", 5, 5, true);
        p.keywords = {"!manhole", "manholes", "open drain", "uncovered", "cover missing", "missing cover", "open manhole", "slab broken", "broken slab",
                      "overflowing drain", "drain overflow*", "no cover", "lid", "fell into", "fall into", "gutter open", "open gutter", "gutter is open",
                      "drain is open", "fell in"};
        p.explanation = "An uncovered manhole or drain is a fall hazard that can cause serious injury, especially at night or in floodwater.";
        p.causes = {"Stolen or broken cover", "Collapsed slab", "Heavy flow lifting the cover"};
        p.immediate = {"Barricade the opening and place visible warning markers within the hour - reflective or lighted at night.",
                       "Fit a temporary cover until the permanent one arrives."};
        p.fieldWork = {"Install a proper replacement cover or slab (a theft-resistant or FRP cover where theft is common).",
                       "Inspect the neighbouring covers on the road for the same problem."};
        p.verify = {"Check the cover is seated, level and takes a person's weight.", "Return after dark to check visibility."};
        p.prevention = {"Report cover theft to the police.", "Add the road to the periodic manhole inspection list."};
        p.resources = {"Barricades, cones and warning lights", "Replacement covers and slabs", "Repair crew"};
        p.citizenSafety = {"Keep well away from the opening and keep children and pets away.",
                           "If it is safe, mark it with a visible object (branch, cone) and warn others, but do not stand in the road.",
                           "Call 112 immediately if someone has fallen in; do not climb in to help."};
        p.citizenTips = {"Say exactly where the cover is missing (landmark and side of road).", "Mention if the road is busy or dark."};
        p.resolvedNote = "The opening has been covered and made safe.";
        v.push_back(p);
    }
    {
        Playbook p = makeGeneral("DRAIN_GENERAL", "DRAINAGE", "General drainage issue");
        p.explanation = "A drainage problem that does not match a specific pattern yet.";
        p.causes = {"Blockage, capacity or structural cause - the inspection will decide"};
        p.immediate = {"Send a drainage inspector to check the flow and the nearest inlet and outfall."};
        p.fieldWork = {"Clear or repair as per the inspection."};
        p.verify = {"Confirm water drains freely afterwards."};
        p.prevention = {"Log the drain for the pre-monsoon desilting round."};
        p.resources = {"Desilting crew", "Pump on standby"};
        p.citizenSafety = {"Stay away from standing water and open drains."};
        p.citizenTips = {"Say how long water stays after rain and where it collects."};
        p.resolvedNote = "The drainage issue at this location has been addressed.";
        v.push_back(p);
    }
}

std::vector<Playbook> build() {
    std::vector<Playbook> v;
    addTraffic(v);
    addGarbage(v);
    addElectricity(v);
    addWater(v);
    addRoads(v);
    addDrainage(v);
    return v;
}

} // namespace

const std::vector<Playbook>& playbooks() {
    static const std::vector<Playbook> all = build();
    return all;
}

const Playbook* playbookById(const std::string& id) {
    for (const auto& p : playbooks())
        if (p.id == id) return &p;
    return nullptr;
}

const Playbook* generalPlaybook(const std::string& categoryCode) {
    for (const auto& p : playbooks())
        if (p.general && p.category == categoryCode) return &p;
    return nullptr;
}

} // namespace ai
} // namespace scms
