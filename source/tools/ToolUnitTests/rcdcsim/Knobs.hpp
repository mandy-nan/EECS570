/*
  RCDC-sim: A Relaxed Consistency Deterministic Computer simulator
  Copyright 2011 University of Washington

  Contributed by Joseph Devietti

This file is part of RCDC-sim.

RCDC-sim is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RCDC-sim is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RCDC-sim.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * String constants used for specifying and identifying command line knobs.
 * You should always access the value of a via these constants to avoid typo errors,
 * since boost::program_options only does dynamic checks.
 */

#ifndef KNOBS_HPP_
#define KNOBS_HPP_

#define KnobStatsFile "statsfile"
#define KnobToSimulatorFifo "tosim-fifo"

#define KnobScheme "scheme"
#define KnobWorkload "workload"
#define KnobThreads "threads"
#define KnobInput "input"

#define KnobCores "cores"

#define KnobIgnoreStackRefs "ignore-stack"

// caches
#define KnobBlockSize "blocksize"
#define KnobL1Size "l1-size"
#define KnobL1Assoc "l1-assoc"
#define KnobUseL2 "use-l2"
#define KnobL2Size "l2-size"
#define KnobL2Assoc "l2-assoc"
#define KnobUseL3 "use-l3"
#define KnobL3Size "l3-size"
#define KnobL3Assoc "l3-assoc"

// RCDC stuff
#define KnobTSO "det-tso"
#define KnobHB "det-hb"
#define KnobNondet "nondet"
#define KnobQuantumSize "quantum-size"
#define KnobSmartQuantumBuilding "smart-qb"

#endif /* KNOBS_HPP_ */
