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

#include "rcdcsim.hpp"
#include "Event.hpp"

int main( int argc, char** argv ) {

  if ( argc < 3 ) {
    cout << "usage: " << argv[0] << " SOURCE DESTINATIONS..." << endl;
    return 1;
  }

  const char* sourcePipe = argv[1];

  ifstream sourceFifo;
  sourceFifo.open( sourcePipe, ios::binary );
  assert( sourceFifo.good() );

  vector<ofstream*> destFifos;
  for ( int i = 2; i < argc; i++ ) {
    ofstream* dest = new ofstream;
    dest->open( argv[i], ios::binary );
    destFifos.push_back( dest );
  }

  Event e;
  char* p = (char*) &e;
  vector<ofstream*>::iterator os;

  while ( true ) {

    unsigned totalBytesRead = 0;
    while ( totalBytesRead < sizeof(Event) ) {
      // read Event
      assert( sourceFifo.good() );
      sourceFifo.read( p + totalBytesRead, sizeof(Event) - totalBytesRead );
      unsigned bytesRead = sourceFifo.gcount();
      totalBytesRead += bytesRead;

      if ( 0 == bytesRead ) { // no more events
        assert( sourceFifo.eof() );
        goto CLEANUP;
      }
    }

    assert( totalBytesRead == sizeof(Event) );

    // multicast Event to all destinations
    for ( os = destFifos.begin(); os != destFifos.end(); os++ ) {
      assert( (*os)->good() );
      ( *os )->write( p, sizeof(Event) );
    }

  } // end while(true)

CLEANUP: //
  sourceFifo.close();
  for ( os = destFifos.begin(); os != destFifos.end(); os++ ) {
    ( *os )->flush();
    ( *os )->close();
  }

  return 0;
}
