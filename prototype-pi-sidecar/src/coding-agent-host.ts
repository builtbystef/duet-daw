// PROTOTYPE — dependency-footprint comparator only. Do not ship.
import { createAgentSession } from "@earendil-works/pi-coding-agent";

if (typeof createAgentSession !== "function") throw new Error("SDK import failed");
process.stdout.write("coding-agent SDK loaded\n");
