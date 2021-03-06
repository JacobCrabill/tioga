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
#include "MeshBlock.h"
#include <assert.h>
#define ROW 0
#define COLUMN 1

extern "C" 
{
  void computeNodalWeights(double xv[8][3],double *xp,double frac[8],int nvert);
//  void writeqnode_(int *myid,double *qnodein,int *qnodesize);
}

void MeshBlock::setCartIblanks(void)
{
  int i,j,m,icount;
  if (ihigh) 
    {
      m=0;
      //
      for(i=0;i<nreceptorCellsCart;i++)
	{
	  icount=0;
	  for(j=0;j<pointsPerCell[i];j++)
	    if (donorIdCart[m++] !=-1) icount++;
	  if (icount==pointsPerCell[i]) iblank_cell[ctag_cart[i]]=-1;
	}
      //
    }
  else
    {
      m=0;
      for(i=0;i<nnodes;i++) 
	{
	  if (pickedCart[i]==1) 
	    {
	      if (donorIdCart[m++] !=-1) iblank[i]=-1;
	    }
	}
    }
}
  
void MeshBlock::getUnresolvedMandatoryReceptors(void)
{
  int i,j,k,m,n,nvert,i3,fcount;
  int inode[8];
  int *iflag;
  iflag=(int *) malloc(sizeof(int) *ncells);
  if (pickedCart !=NULL) free(pickedCart);
  pickedCart=(int *) malloc(sizeof(int)*nnodes);

  for(i=0;i<ncells;i++) iflag[i]=0;
  for(i=0;i<nnodes;i++) pickedCart[i]=0;

  k=0;
  for(n=0;n<ntypes;n++)
    {
      nvert=nv[n];
      for(i=0;i<nc[n];i++)
	{
	  fcount=0;
	  for(m=0;m<nvert;m++)
	    {
	      inode[m]=vconn[n][nvert*i+m]-BASE;
	      if (nodeRes[inode[m]]==BIGVALUE) fcount++;
	    }
	  if (fcount==nvert && iblank_cell[k]==1) 
	    {
	      iflag[k]=1;
	      for(m=0;m<nvert;m++)
		pickedCart[inode[m]]=1;
	    }
	  k++;
	}
    }
  //
  if (ctag_cart!=NULL) free(ctag_cart);
  ctag_cart=(int *)malloc(sizeof(int)*ncells);
  nreceptorCellsCart=0;
  for(i=0;i<ncells;i++)
    if (iflag[i]==-1) ctag_cart[nreceptorCellsCart++]=i+1;
  //
  if (ihigh) 
    {
      if (pointsPerCell!=NULL) free(pointsPerCell);
      pointsPerCell=(int *)malloc(sizeof(int)*nreceptorCellsCart);
      //
      maxPointsPerCell=0;
      ntotalPointsCart=0;
      //
      for(i=0;i<nreceptorCellsCart;i++)
	{
	  get_nodes_per_cell(&(ctag_cart[i]),&(pointsPerCell[i]));
	  ntotalPointsCart+=pointsPerCell[i];
	  maxPointsPerCell=max(maxPointsPerCell,pointsPerCell[i]);
      }
      //
      if (rxyzCart !=NULL) free(rxyzCart);
      if (donorIdCart !=NULL) free(donorIdCart);
      //printf("getInternalNodes : %d %d\n",myid,ntotalPoints);
      rxyzCart=(double *)malloc(sizeof(double)*ntotalPointsCart*3);
      donorIdCart=(int *)malloc(sizeof(int)*ntotalPointsCart);
      //
      m=0;
      for(i=0;i<nreceptorCellsCart;i++)
	{
	  get_receptor_nodes(&(ctag_cart[i]),&(pointsPerCell[i]),&(rxyzCart[m]));
	  m+=(3*pointsPerCell[i]);
	}
    }
  else
    {
      ntotalPointsCart=0;      
      for(i=0;i<nnodes;i++) 
	{
	  if (pickedCart[i]==1) 
	    {
	      ntotalPointsCart++;
	    }
	}
      if (rxyzCart !=NULL) free(rxyzCart);
      if (donorIdCart !=NULL) free(donorIdCart);
      rxyzCart=(double *)malloc(sizeof(double)*ntotalPointsCart*3);
      donorIdCart=(int *)malloc(sizeof(int)*ntotalPointsCart);
      m=0;
      for(i=0;i<nnodes;i++)
	if (pickedCart[i]) 
	  {
	    i3=3*i;
	    for(j=0;j<3;j++)
	      rxyzCart[m++]=x[i3+j];
	  }
    }
 free(iflag);
}

void MeshBlock::writeOBB2(OBB * obc, int bid)
{
  FILE *fp;
  char intstring[7];
  char fname[80];
  int l,k,j,m,il,ik,ij;
  REAL xx[3];

  sprintf(intstring,"%d",100000+bid);
  sprintf(fname,"cbox%s.dat",&(intstring[1]));
  fp=fopen(fname,"w");
  fprintf(fp,"TITLE =\"Box file\"\n");
  fprintf(fp,"VARIABLES=\"X\",\"Y\",\"Z\"\n");
  fprintf(fp,"ZONE T=\"VOL_MIXED\",N=%d E=%d ET=BRICK, F=FEPOINT\n",8,
	  1);

  for(l=0;l<2;l++)
    {
      il=2*(l%2)-1;
      for(k=0;k<2;k++)
	{
	  ik=2*(k%2)-1;
	  for(j=0;j<2;j++)
	    {
	      ij=2*(j%2)-1;
	      xx[0]=xx[1]=xx[2]=0;
	      for(m=0;m<3;m++)
		xx[m]=obc->xc[m]+ij*obc->vec[0][m]*obc->dxc[0]
		  +ik*obc->vec[1][m]*obc->dxc[1]
		  +il*obc->vec[2][m]*obc->dxc[2];	      
	      fprintf(fp,"%f %f %f\n",xx[0],xx[1],xx[2]);
	    }
	}
    }
  fprintf(fp,"1 2 4 3 5 6 8 7\n");
  fprintf(fp,"%e %e %e\n",obc->xc[0],obc->xc[1],obc->xc[2]);
  for(k=0;k<3;k++)
   fprintf(fp,"%e %e %e\n",obc->vec[0][k],obc->vec[1][k],obc->vec[2][k]);
  fprintf(fp,"%e %e %e\n",obc->dxc[0],obc->dxc[1],obc->dxc[2]);
  fclose(fp);
}


void MeshBlock::findInterpListCart(void)
{
  int irecord,i3,i,j,n,m;
  double xp[3];
  double xv[8][3];
  double frac[8];
  int inode[8];
  int nvert;
  int isum,interpCount;
  int procid,pointid,localid;

  if (interpListCart)
  {
    free(interpListCart);
    interpListCartSize=0;
  }
  for(irecord=0;irecord<nsearch;irecord++)
    if (donorId[irecord]!=-1) interpListCartSize++;
  interpListCart=(INTERPLIST *)malloc(sizeof(INTERPLIST)*interpListCartSize);
  interpCount=0;
  for(irecord=0;irecord<nsearch;irecord++)
    if (donorId[irecord]!=-1) 
      {
	i3=3*irecord;
	procid=isearch[i3];
	localid=isearch[i3+1];
	pointid=isearch[i3+2];
	xp[0]=xsearch[i3];
	xp[1]=xsearch[i3+1];
	xp[2]=xsearch[i3+2];
	//
	isum=0;
	for(n=0;n<ntypes;n++)
	  {
	    isum+=nc[n];
	    if (donorId[irecord] < isum)
	      {
		i=donorId[irecord]-(isum-nc[n]);
		break;
	      }
	  }
	nvert=nv[n];
	for(m=0;m<nvert;m++)
	  {
	    inode[m]=vconn[n][nvert*i+m]-BASE;
	    i3=3*inode[m];
	    for(j=0;j<3;j++)
	      xv[m][j]=x[i3+j];
	  }
	//  
	computeNodalWeights(xv,xp,frac,nvert);
	//
	interpListCart[interpCount].receptorInfo[0]=procid;
	interpListCart[interpCount].receptorInfo[1]=pointid;
	interpListCart[interpCount].receptorInfo[2]=localid;
	interpListCart[interpCount].nweights=nvert;
	interpListCart[interpCount].cancel=0;
	//
  interpListCart[interpCount].inode.resize(nvert);
  interpListCart[interpCount].weights.resize(nvert);
	for(m=0;m<nvert;m++)
	  {
	    interpListCart[interpCount].inode[m]=inode[m];
	    interpListCart[interpCount].weights[m]=frac[m];
	  }
	interpCount++;
      }
  ninterpCart=interpCount;
  assert((ninterpCart==interpCount));
}    


void MeshBlock::getInterpolatedSolutionAMR(int *nints,int *nreals,int **intData,double **realData,double *q,
					int nvar, int interptype)
{
  int i;
  int k,m,inode;
  double weight;
  double *qq;
  int icount,dcount;
  //
  qq=(double *)malloc(sizeof(double)*nvar);
  //
  (*nints)=(*nreals)=0;
  for(i=0;i<ninterp;i++)
    {
      if (!interpList[i].cancel)
	{
	  (*nints)++;
	  (*nreals)=(*nreals)+nvar;
	}
    }
  for(i=0;i<ninterpCart;i++)
    {
      if (!interpListCart[i].cancel)
	{
	  (*nints)++;
	  (*nreals)=(*nreals)+nvar;
	}
    }	          
  if ((*nints)==0) return;
  //
  (*intData)=(int *)malloc(sizeof(int)*3*(*nints));
  (*realData)=(double *)malloc(sizeof(double)*(*nreals));
  icount=dcount=0;
  //
  if (interptype==ROW)
    {    
      for(i=0;i<ninterp;i++)
	{
	  if (!interpList[i].cancel)
	    {
	      for(k=0;k<nvar;k++) qq[k]=0;
	      for(m=0;m<interpList[i].nweights;m++)
		{
		  inode=interpList[i].inode[m];
		  weight=interpList[i].weights[m];
		  if (weight < 0 || weight > 1.0) {
                    traced(weight);
                    printf("warning: weights are not convex 3\n");
                   }
		  for(k=0;k<nvar;k++)
		    qq[k]+=q[inode*nvar+k]*weight;
		}
	      (*intData)[icount++]=interpList[i].receptorInfo[0];
	      (*intData)[icount++]=-1;
	      (*intData)[icount++]=interpList[i].receptorInfo[1];
	      for(k=0;k<nvar;k++)
		(*realData)[dcount++]=qq[k];
	    }
	}
      for(i=0;i<ninterpCart;i++)
	{
	  if (!interpListCart[i].cancel)
	    {
	      for(k=0;k<nvar;k++) qq[k]=0;
	      for(m=0;m<interpListCart[i].nweights;m++)
		{
		  inode=interpListCart[i].inode[m];
		  weight=interpListCart[i].weights[m];
		  if (weight < 0 || weight > 1.0) {
                    traced(weight);
                    printf("warning: weights are not convex 4\n");
                   }
		  for(k=0;k<nvar;k++)
                    {
		     qq[k]+=q[inode*nvar+k]*weight;
                     // if (myid==0 && dcount==0) {
		     //   printf("nsu3d/interp: %d %d %f %f\n",k,inode,weight,q[inode*nvar+k]);
		     // }
		    }
		}
	      //writeqnode_(&myid,qq,&nvar);
	      (*intData)[icount++]=interpListCart[i].receptorInfo[0];
	      (*intData)[icount++]=interpListCart[i].receptorInfo[2];
	      (*intData)[icount++]=interpListCart[i].receptorInfo[1];
	      for(k=0;k<nvar;k++)
		(*realData)[dcount++]=qq[k];
	    }
	}
    }
  else if (interptype==COLUMN)
    {
      for(i=0;i<ninterp;i++)
	{
	  if (!interpList[i].cancel)
	    {
	      for(k=0;k<nvar;k++) qq[k]=0;
	      for(m=0;m<interpList[i].nweights;m++)
		{
		  inode=interpList[i].inode[m];
		  weight=interpList[i].weights[m];
		  for(k=0;k<nvar;k++)
		    qq[k]+=q[k*nnodes+inode]*weight;
		}
	      (*intData)[icount++]=interpList[i].receptorInfo[0];
	      (*intData)[icount++]=-1;
	      (*intData)[icount++]=interpList[i].receptorInfo[1];
	      for(k=0;k<nvar;k++)
		(*realData)[dcount++]=qq[k];
	    }
	}
      for(i=0;i<ninterpCart;i++)
	{
	  if (!interpListCart[i].cancel)
	    {
	      for(k=0;k<nvar;k++) qq[k]=0;
	      for(m=0;m<interpListCart[i].nweights;m++)
		{
		  inode=interpListCart[i].inode[m];
		  weight=interpListCart[i].weights[m];
		  for(k=0;k<nvar;k++)
		    qq[k]+=q[k*nnodes+inode]*weight;
		}
	      (*intData)[icount++]=interpListCart[i].receptorInfo[0];
	      (*intData)[icount++]=interpListCart[i].receptorInfo[1];
	      (*intData)[icount++]=interpListCart[i].receptorInfo[2];
	      for(k=0;k<nvar;k++)
		(*realData)[dcount++]=qq[k];
	    }
	}
    }
}
