//
// This file is part of the Tioga software library
//
// Tioga  is a tool for overset grid assembly on parallel distributed systems
// Copyright (C) 2015 Jay Sitaraman
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#include "tioga.h"

void tioga::exchangeSearchData(void)
{
  // Get the processor map for sending and receiving
  int nsend, nrecv;
  int *sndMap, *rcvMap;

  pc->getMap(&nsend,&nrecv,&sndMap,&rcvMap);

  // Create packets to send and receive points, and initialize them to zero
  PACKET *sndPack = (PACKET *)malloc(sizeof(PACKET)*nsend);
  PACKET *rcvPack = (PACKET *)malloc(sizeof(PACKET)*nrecv);

  for (int i = 0; i < nsend; i++)
  {
    sndPack[i].nints = 0;
    sndPack[i].nreals = 0;
    sndPack[i].intData = NULL;
    sndPack[i].realData = NULL;
  }

  for (int i = 0; i < nrecv; i++)
  {
    rcvPack[i].nints = 0;
    rcvPack[i].nreals = 0;
    rcvPack[i].intData = NULL;
    rcvPack[i].realData = NULL;
  }

  // Find all mesh nodes on this grid which lie within the bounding box of each
  // other grid
  for (int k = 0; k < nsend; k++)
    mb->getQueryPoints(&obblist[k],
		       &sndPack[k].nints,&sndPack[k].intData,
		       &sndPack[k].nreals,&sndPack[k].realData);

  // Exchange the data
  pc->sendRecvPackets(sndPack,rcvPack);

  // now assort the data into the search list arrays
  mb->nsearch = 0;
  for (int k = 0; k < nrecv; k++)
    mb->nsearch += rcvPack[k].nints;

  mb->xsearch.resize(3*mb->nsearch);
  mb->isearch.resize(2*mb->nsearch);
  mb->donorId.resize(mb->nsearch);
  mb->tagsearch.resize(mb->nsearch);
  if (ihigh)
  {
    mb-> rst.resize(3*mb->nsearch); /// NEEDED HERE?
    std::fill(mb->rst.data(), mb->rst.data()+3*mb->nsearch, 0.0);
  }

  // now fill the query point arrays
  int icount = 0;
  int dcount = 0;
  int tcount = 0;
  for (int k = 0; k < nrecv; k++)
  {
    int l = 0;
    for (int j = 0; j < rcvPack[k].nints; j++)
    {
      mb->isearch[icount++] = k;
      mb->isearch[icount++] = rcvPack[k].intData[j];
      mb->xsearch[dcount++] = rcvPack[k].realData[l++];
      mb->xsearch[dcount++] = rcvPack[k].realData[l++];
      mb->xsearch[dcount++] = rcvPack[k].realData[l++];
      mb->tagsearch[tcount++] = obblist[k].meshtag;
    }
  }

  for (int i = 0; i < nsend; i++)
  {
    free(sndPack[i].intData);
    free(sndPack[i].realData);
  }

  for (int i = 0; i < nrecv; i++)
  {
    free(rcvPack[i].intData);
    free(rcvPack[i].realData);
  }

  free(sndPack);
  free(rcvPack);
}
  
//
// routine for extra query points
// have to unify both routines here 
// FIX later ...
//
void tioga::exchangePointSearchData(void)
{
  // get the processor map for sending
  // and receiving
  int nsend, nrecv;
  int *sndMap, *rcvMap;
  pc->getMap(&nsend,&nrecv,&sndMap,&rcvMap);

  // create packets to send and receive
  // and initialize them to zero
  PACKET *sndPack = (PACKET *)malloc(sizeof(PACKET)*nsend);
  PACKET *rcvPack = (PACKET *)malloc(sizeof(PACKET)*nrecv);

  for (int i = 0; i < nsend; i++)
  {
    sndPack[i].nints = sndPack[i].nreals = 0;
    sndPack[i].intData=NULL;
    sndPack[i].realData=NULL;
  }

  for (int i = 0; i < nrecv; i++)
  {
    rcvPack[i].nints = rcvPack[i].nreals = 0;
    rcvPack[i].intData = NULL;
    rcvPack[i].realData = NULL;
  }

  // now get data for each packet [all of our fringe points which touch rank k's obb]
  for (int k = 0; k < nsend; k++)
    mb->getExtraQueryPoints(&obblist[k], sndPack[k].nints, sndPack[k].intData,
                            sndPack[k].nreals, sndPack[k].realData);

  // exchange the data
  pc->sendRecvPackets(sndPack,rcvPack);

  // now sort the data into the search list arrays
  int nsearch_prev = mb->nsearch;
  mb->nsearch=0;
  for (int k = 0; k < nrecv; k++)
    mb->nsearch += rcvPack[k].nints;

  // allocate query point storage
  mb->xsearch.resize(3*mb->nsearch);
  mb->isearch.resize(2*mb->nsearch);
  mb->donorId.resize(mb->nsearch);
  if (mb->nsearch != nsearch_prev) // Keep previous r,s,t values for checkContainment()
  {
    mb->rst.resize(3*mb->nsearch);
    std::fill(mb->rst.data(), mb->rst.data()+3*mb->nsearch, 0.0);
  }

  // now fill the query point arrays
  int count = 0;
  for (int k = 0; k < nrecv; k++)
  {
    for (int j = 0; j < rcvPack[k].nints; j++)
    {
      mb->isearch[2*count+0] = k;
      mb->isearch[2*count+1] = rcvPack[k].intData[j];
      mb->xsearch[3*count+0] = rcvPack[k].realData[3*j];
      mb->xsearch[3*count+1] = rcvPack[k].realData[3*j+1];
      mb->xsearch[3*count+2] = rcvPack[k].realData[3*j+2];
      count++;
    }
  }

  for (int i = 0; i <nsend; i++)
  {
    if (sndPack[i].nints > 0) free(sndPack[i].intData);
    if (sndPack[i].nreals >0) free(sndPack[i].realData);
  }

  for (int i = 0; i < nrecv; i++)
  {
    if (rcvPack[i].nints > 0) free(rcvPack[i].intData);
    if (rcvPack[i].nreals >0) free(rcvPack[i].realData);
  }

  free(sndPack);
  free(rcvPack);
}
  
