/* ----------------------------------------------------------------------
    This is the

    ██╗     ██╗ ██████╗  ██████╗  ██████╗ ██╗  ██╗████████╗███████╗
    ██║     ██║██╔════╝ ██╔════╝ ██╔════╝ ██║  ██║╚══██╔══╝██╔════╝
    ██║     ██║██║  ███╗██║  ███╗██║  ███╗███████║   ██║   ███████╗
    ██║     ██║██║   ██║██║   ██║██║   ██║██╔══██║   ██║   ╚════██║
    ███████╗██║╚██████╔╝╚██████╔╝╚██████╔╝██║  ██║   ██║   ███████║
    ╚══════╝╚═╝ ╚═════╝  ╚═════╝  ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚══════╝®

    DEM simulation engine, released by
    DCS Computing Gmbh, Linz, Austria
    http://www.dcs-computing.com, office@dcs-computing.com

    LIGGGHTS® is part of CFDEM®project:
    http://www.liggghts.com | http://www.cfdem.com

    Core developer and main author:
    Christoph Kloss, christoph.kloss@dcs-computing.com

    LIGGGHTS® is open-source, distributed under the terms of the GNU Public
    License, version 2 or later. It is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You should have
    received a copy of the GNU General Public License along with LIGGGHTS®.
    If not, see http://www.gnu.org/licenses . See also top-level README
    and LICENSE files.

    LIGGGHTS® and CFDEM® are registered trade marks of DCS Computing GmbH,
    the producer of the LIGGGHTS® software and the CFDEM®coupling software
    See http://www.cfdem.com/terms-trademark-policy for details.

-------------------------------------------------------------------------
    Contributing author and copyright for this file:
    This file is from LAMMPS, but has been modified. Copyright for
    modification:

    Copyright 2012-     DCS Computing GmbH, Linz
    Copyright 2009-2012 JKU Linz

    Copyright of original file:
    LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
    http://lammps.sandia.gov, Sandia National Laboratories
    Steve Plimpton, sjplimp@sandia.gov

    Copyright (2003) Sandia Corporation.  Under the terms of Contract
    DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
    certain rights in this software.  This software is distributed under
    the GNU General Public License.
------------------------------------------------------------------------- */

#include <cmath>
#include <stdlib.h>
#include <string.h>
#include "atom_vec_sphere.h"
#include "atom.h"
#include "comm.h"
#include "domain.h"
#include "modify.h"
#include "force.h"
#include "fix.h"
#include "fix_adapt.h"
#include "math_const.h"
#include "memory.h"
#include "error.h"
#include "domain_wedge.h"

using namespace LAMMPS_NS;
using namespace MathConst;

#define DELTA 10000

/* ---------------------------------------------------------------------- */

AtomVecSphere::AtomVecSphere(LAMMPS *lmp) : AtomVec(lmp)
{
  molecular = 0;

  comm_x_only = 1;
  comm_f_only = 0;
  size_forward = 3;
  size_reverse = 6;
//Lwx
  size_border = 15; //9;12;
  size_velocity = 6;
  size_data_atom = 13;  //7;10;
  size_data_vel = 7;
  xcol_data = 11;    //5;8;
//
  atom->sphere_flag = 1;
  atom->radius_flag = atom->rmass_flag = atom->omega_flag = atom->density_flag =
    atom->torque_flag = 1;
//
  atom->Lwx_flag = 1;
  atom->Lwx2_flag = 1;
//
}

/* ---------------------------------------------------------------------- */

void AtomVecSphere::init()
{
  AtomVec::init();

  // set radvary if particle diameters are time-varying due to fix adapt

  radvary = 0;
  comm_x_only = 1;
  size_forward = 3;

  // set radvary if particle diameters are time-varying due to some fix
  for (int i = 0; i < modify->nfix; i++)
  {
      if (modify->fix[i]->rad_mass_vary_flag) {
        radvary = 1;
        size_forward = 7; 
        comm_x_only = 1;
      }
  }

  if(radvary) atom->radvary_flag = 1;
}

/* ----------------------------------------------------------------------
   grow atom arrays
   n = 0 grows arrays by DELTA
   n > 0 allocates arrays to size n
------------------------------------------------------------------------- */

void AtomVecSphere::grow(int n)
{
  if (n == 0) nmax += DELTA;
  else nmax = n;
  atom->nmax = nmax;
  if (nmax < 0 || nmax > MAXSMALLINT)
    error->one(FLERR,"Per-processor system is too big");

  tag = memory->grow(atom->tag,nmax,"atom:tag");
  type = memory->grow(atom->type,nmax,"atom:type");
  mask = memory->grow(atom->mask,nmax,"atom:mask");
  image = memory->grow(atom->image,nmax,"atom:image");
  x = memory->grow(atom->x,nmax,3,"atom:x");
  v = memory->grow(atom->v,nmax,3,"atom:v");
  f = memory->grow(atom->f,nmax*comm->nthreads,3,"atom:f");
//
  Lwx = memory->grow(atom->Lwx, nmax, 3, "atom:Lwx");
  Lwx2 = memory->grow(atom->Lwx2, nmax, 3, "atom:Lwx2");
//

  radius = memory->grow(atom->radius,nmax,"atom:radius");
  density = memory->grow(atom->density,nmax,"atom:density"); 
  rmass = memory->grow(atom->rmass,nmax,"atom:rmass");
  omega = memory->grow(atom->omega,nmax,3,"atom:omega");
  torque = memory->grow(atom->torque,nmax*comm->nthreads,3,"atom:torque");

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      modify->fix[atom->extra_grow[iextra]]->grow_arrays(nmax);
}

/* ----------------------------------------------------------------------
   reset local array ptrs
------------------------------------------------------------------------- */

void AtomVecSphere::grow_reset()
{
  tag = atom->tag; type = atom->type;
  mask = atom->mask; image = atom->image;
  x = atom->x; v = atom->v; f = atom->f;
  radius = atom->radius; density = atom->density; rmass = atom->rmass; 
  omega = atom->omega; torque = atom->torque;
//
  Lwx = atom->Lwx;
  Lwx2 = atom->Lwx2;
//
}

/* ----------------------------------------------------------------------
   copy atom I info to atom J
------------------------------------------------------------------------- */

void AtomVecSphere::copy(int i, int j, int delflag)
{
  tag[j] = tag[i];
  type[j] = type[i];
  mask[j] = mask[i];
  image[j] = image[i];
  x[j][0] = x[i][0];
  x[j][1] = x[i][1];
  x[j][2] = x[i][2];
  v[j][0] = v[i][0];
  v[j][1] = v[i][1];
  v[j][2] = v[i][2];

  radius[j] = radius[i];
  rmass[j] = rmass[i];
  density[j] = density[i]; 
//
  Lwx[j][0] = Lwx[i][0];
  Lwx[j][1] = Lwx[i][1];
  Lwx[j][2] = Lwx[i][2];
  Lwx2[j][0] = Lwx2[i][0];
  Lwx2[j][1] = Lwx2[i][1];
  Lwx2[j][2] = Lwx2[i][2];
//
  omega[j][0] = omega[i][0];
  omega[j][1] = omega[i][1];
  omega[j][2] = omega[i][2];

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      modify->fix[atom->extra_grow[iextra]]->copy_arrays(i,j,delflag);
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::pack_comm(int n, int *list, double *buf,
                               int pbc_flag, int *pbc)
{
  int i,j,m;
  double dx,dy,dz;

  if (radvary == 0) {
    m = 0;
    if (pbc_flag == 0) {
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0];
        buf[m++] = x[j][1];
        buf[m++] = x[j][2];
      }
    } else {
      if (domain->triclinic == 0) {
        dx = pbc[0]*domain->xprd;
        dy = pbc[1]*domain->yprd;
        dz = pbc[2]*domain->zprd;
      } else {
        dx = pbc[0]*domain->xprd + pbc[5]*domain->xy + pbc[4]*domain->xz;
        dy = pbc[1]*domain->yprd + pbc[3]*domain->yz;
        dz = pbc[2]*domain->zprd;
      }
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0] + dx;
        buf[m++] = x[j][1] + dy;
        buf[m++] = x[j][2] + dz;
      }
    }

  } else {
    m = 0;
    if (pbc_flag == 0) {
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0];
        buf[m++] = x[j][1];
        buf[m++] = x[j][2];
        buf[m++] = ubuf(type[j]).d; 
        buf[m++] = radius[j];
        buf[m++] = rmass[j];
        buf[m++] = density[j]; 
//
        buf[m++] = Lwx[j][0];
        buf[m++] = Lwx[j][1];
        buf[m++] = Lwx[j][2];
        buf[m++] = Lwx2[j][0];
        buf[m++] = Lwx2[j][1];
        buf[m++] = Lwx2[j][2];
//
      }
    } else {
      if (domain->triclinic == 0) {
        dx = pbc[0]*domain->xprd;
        dy = pbc[1]*domain->yprd;
        dz = pbc[2]*domain->zprd;
      } else {
        dx = pbc[0]*domain->xprd + pbc[5]*domain->xy + pbc[4]*domain->xz;
        dy = pbc[1]*domain->yprd + pbc[3]*domain->yz;
        dz = pbc[2]*domain->zprd;
      }
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0] + dx;
        buf[m++] = x[j][1] + dy;
        buf[m++] = x[j][2] + dz;
        buf[m++] = ubuf(type[j]).d; 
        buf[m++] = radius[j];
        buf[m++] = rmass[j];
        buf[m++] = density[j]; 
//
        buf[m++] = Lwx[j][0];
        buf[m++] = Lwx[j][1];
        buf[m++] = Lwx[j][2];
        buf[m++] = Lwx2[j][0];
        buf[m++] = Lwx2[j][1];
        buf[m++] = Lwx2[j][2];
//
      }
    }
  }

  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::pack_comm_vel(int n, int *list, double *buf,
                                   int pbc_flag, int *pbc)
{
  if(dynamic_cast<DomainWedge*>(domain))
    return pack_comm_vel_wedge(n,list,buf,pbc_flag,pbc);

  int i,j,m;

  double dx,dy,dz,dvx,dvy,dvz;

  if (radvary == 0) {
    m = 0;
    if (pbc_flag == 0) {
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0];
        buf[m++] = x[j][1];
        buf[m++] = x[j][2];
        buf[m++] = v[j][0];
        buf[m++] = v[j][1];
        buf[m++] = v[j][2];
        buf[m++] = omega[j][0];
        buf[m++] = omega[j][1];
        buf[m++] = omega[j][2];
      }
    } else {
      if (domain->triclinic == 0) {
        dx = pbc[0]*domain->xprd;
        dy = pbc[1]*domain->yprd;
        dz = pbc[2]*domain->zprd;
      } else {
        dx = pbc[0]*domain->xprd + pbc[5]*domain->xy + pbc[4]*domain->xz;
        dy = pbc[1]*domain->yprd + pbc[3]*domain->yz;
        dz = pbc[2]*domain->zprd;
      }
      if (!deform_vremap) {
        for (i = 0; i < n; i++) {
          j = list[i];
          buf[m++] = x[j][0] + dx;
          buf[m++] = x[j][1] + dy;
          buf[m++] = x[j][2] + dz;
          buf[m++] = v[j][0];
          buf[m++] = v[j][1];
          buf[m++] = v[j][2];
          buf[m++] = omega[j][0];
          buf[m++] = omega[j][1];
          buf[m++] = omega[j][2];
        }
      } else {
        dvx = pbc[0]*h_rate[0] + pbc[5]*h_rate[5] + pbc[4]*h_rate[4];
        dvy = pbc[1]*h_rate[1] + pbc[3]*h_rate[3];
        dvz = pbc[2]*h_rate[2];
        for (i = 0; i < n; i++) {
          j = list[i];
          buf[m++] = x[j][0] + dx;
          buf[m++] = x[j][1] + dy;
          buf[m++] = x[j][2] + dz;
          if (mask[i] & deform_groupbit) {
            buf[m++] = v[j][0] + dvx;
            buf[m++] = v[j][1] + dvy;
            buf[m++] = v[j][2] + dvz;
          } else {
            buf[m++] = v[j][0];
            buf[m++] = v[j][1];
            buf[m++] = v[j][2];
          }
          buf[m++] = omega[j][0];
          buf[m++] = omega[j][1];
          buf[m++] = omega[j][2];
        }
      }
    }

  } else {
    m = 0;
    if (pbc_flag == 0) {
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0];
        buf[m++] = x[j][1];
        buf[m++] = x[j][2];
        buf[m++] = ubuf(type[j]).d; 
        buf[m++] = radius[j];
        buf[m++] = rmass[j];
        buf[m++] = density[j]; 
//
        buf[m++] = Lwx[j][0];
        buf[m++] = Lwx[j][1];
        buf[m++] = Lwx[j][2];
        buf[m++] = Lwx2[j][0];
        buf[m++] = Lwx2[j][1];
        buf[m++] = Lwx2[j][2];
//
        buf[m++] = v[j][0];
        buf[m++] = v[j][1];
        buf[m++] = v[j][2];
        buf[m++] = omega[j][0];
        buf[m++] = omega[j][1];
        buf[m++] = omega[j][2];
      }
    } else {
      if (domain->triclinic == 0) {
        dx = pbc[0]*domain->xprd;
        dy = pbc[1]*domain->yprd;
        dz = pbc[2]*domain->zprd;
      } else {
        dx = pbc[0]*domain->xprd + pbc[5]*domain->xy + pbc[4]*domain->xz;
        dy = pbc[1]*domain->yprd + pbc[3]*domain->yz;
        dz = pbc[2]*domain->zprd;
      }
      if (!deform_vremap) {
        for (i = 0; i < n; i++) {
          j = list[i];
          buf[m++] = x[j][0] + dx;
          buf[m++] = x[j][1] + dy;
          buf[m++] = x[j][2] + dz;
          buf[m++] = ubuf(type[j]).d; 
          buf[m++] = radius[j];
          buf[m++] = rmass[j];
          buf[m++] = density[j]; 
//
          buf[m++] = Lwx[j][0];
          buf[m++] = Lwx[j][1];
          buf[m++] = Lwx[j][2];
          buf[m++] = Lwx2[j][0];
          buf[m++] = Lwx2[j][1];
          buf[m++] = Lwx2[j][2];
//
          buf[m++] = v[j][0];
          buf[m++] = v[j][1];
          buf[m++] = v[j][2];
          buf[m++] = omega[j][0];
          buf[m++] = omega[j][1];
          buf[m++] = omega[j][2];
        }
      } else {
        dvx = pbc[0]*h_rate[0] + pbc[5]*h_rate[5] + pbc[4]*h_rate[4];
        dvy = pbc[1]*h_rate[1] + pbc[3]*h_rate[3];
        dvz = pbc[2]*h_rate[2];
        for (i = 0; i < n; i++) {
          j = list[i];
          buf[m++] = x[j][0] + dx;
          buf[m++] = x[j][1] + dy;
          buf[m++] = x[j][2] + dz;
          buf[m++] = ubuf(type[j]).d; 
          buf[m++] = radius[j];
          buf[m++] = rmass[j];
          buf[m++] = density[j]; 
//
          buf[m++] = Lwx[j][0];
          buf[m++] = Lwx[j][1];
          buf[m++] = Lwx[j][2];
          buf[m++] = Lwx2[j][0];
          buf[m++] = Lwx2[j][1];
          buf[m++] = Lwx2[j][2];
//
          if (mask[i] & deform_groupbit) {
            buf[m++] = v[j][0] + dvx;
            buf[m++] = v[j][1] + dvy;
            buf[m++] = v[j][2] + dvz;
          } else {
            buf[m++] = v[j][0];
            buf[m++] = v[j][1];
            buf[m++] = v[j][2];
          }
          buf[m++] = omega[j][0];
          buf[m++] = omega[j][1];
          buf[m++] = omega[j][2];
        }
      }
    }
  }

  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::pack_comm_hybrid(int n, int *list, double *buf)
{
  int i,j,m;

  if (radvary == 0) return 0;

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    buf[m++] = ubuf(type[j]).d; 
    buf[m++] = radius[j];
    buf[m++] = rmass[j];
    buf[m++] = density[j];
//
    buf[m++] = Lwx[j][0];
    buf[m++] = Lwx[j][1];
    buf[m++] = Lwx[j][2];
    buf[m++] = Lwx2[j][0];
    buf[m++] = Lwx2[j][1];
    buf[m++] = Lwx2[j][2];
//
  }
  return m;
}

/* ---------------------------------------------------------------------- */

void AtomVecSphere::unpack_comm(int n, int first, double *buf)
{
  int i,m,last;

  if (radvary == 0) {
    m = 0;
    last = first + n;
    for (i = first; i < last; i++) {
      x[i][0] = buf[m++];
      x[i][1] = buf[m++];
      x[i][2] = buf[m++];
    }
  } else {
    m = 0;
    last = first + n;
    for (i = first; i < last; i++) {
      x[i][0] = buf[m++];
      x[i][1] = buf[m++];
      x[i][2] = buf[m++];
      type[i] = (int) ubuf(buf[m++]).i; 
      radius[i] = buf[m++];
      rmass[i] = buf[m++];
      density[i] = buf[m++]; 
//
      Lwx[i][0] = buf[m++];
      Lwx[i][1] = buf[m++];
      Lwx[i][2] = buf[m++];
      Lwx2[i][0] = buf[m++];
      Lwx2[i][1] = buf[m++];
      Lwx2[i][2] = buf[m++];
//
    }
  }
}

/* ---------------------------------------------------------------------- */

void AtomVecSphere::unpack_comm_vel(int n, int first, double *buf)
{
  int i,m,last;

  if (radvary == 0) {
    m = 0;
    last = first + n;
    for (i = first; i < last; i++) {
      x[i][0] = buf[m++];
      x[i][1] = buf[m++];
      x[i][2] = buf[m++];
      v[i][0] = buf[m++];
      v[i][1] = buf[m++];
      v[i][2] = buf[m++];
      omega[i][0] = buf[m++];
      omega[i][1] = buf[m++];
      omega[i][2] = buf[m++];
      
    }
  } else {
    m = 0;
    last = first + n;
    for (i = first; i < last; i++) {
      x[i][0] = buf[m++];
      x[i][1] = buf[m++];
      x[i][2] = buf[m++];
      type[i] = (int) ubuf(buf[m++]).i; 
      radius[i] = buf[m++];
      rmass[i] = buf[m++];
      density[i] = buf[m++]; 
//
      Lwx[i][0] = buf[m++];
      Lwx[i][1] = buf[m++];
      Lwx[i][2] = buf[m++];
      Lwx2[i][0] = buf[m++];
      Lwx2[i][1] = buf[m++];
      Lwx2[i][2] = buf[m++];
//
      v[i][0] = buf[m++];
      v[i][1] = buf[m++];
      v[i][2] = buf[m++];
      omega[i][0] = buf[m++];
      omega[i][1] = buf[m++];
      omega[i][2] = buf[m++];
    }
  }
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::unpack_comm_hybrid(int n, int first, double *buf)
{
  int i,m,last;

  if (radvary == 0) return 0;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    type[i] = (int) ubuf(buf[m++]).i; 
    radius[i] = buf[m++];
    rmass[i] = buf[m++];
    density[i] = buf[m++]; 
//
    Lwx[i][0] = buf[m++];
    Lwx[i][1] = buf[m++];
    Lwx[i][2] = buf[m++];
    Lwx2[i][0] = buf[m++];
    Lwx2[i][1] = buf[m++];
    Lwx2[i][2] = buf[m++];
//
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::pack_reverse(int n, int first, double *buf)
{
  int i,m,last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    buf[m++] = f[i][0];
    buf[m++] = f[i][1];
    buf[m++] = f[i][2];
    buf[m++] = torque[i][0];
    buf[m++] = torque[i][1];
    buf[m++] = torque[i][2];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::pack_reverse_hybrid(int n, int first, double *buf)
{
  int i,m,last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    buf[m++] = torque[i][0];
    buf[m++] = torque[i][1];
    buf[m++] = torque[i][2];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

void AtomVecSphere::unpack_reverse(int n, int *list, double *buf)
{
  int i,j,m;

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    
    f[j][0] += buf[m++];
    f[j][1] += buf[m++];
    f[j][2] += buf[m++];
    torque[j][0] += buf[m++];
    torque[j][1] += buf[m++];
    torque[j][2] += buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::unpack_reverse_hybrid(int n, int *list, double *buf)
{
  int i,j,m;

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    torque[j][0] += buf[m++];
    torque[j][1] += buf[m++];
    torque[j][2] += buf[m++];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::pack_border(int n, int *list, double *buf,
                                 int pbc_flag, int *pbc)
{
  int i,j,m;
  double dx,dy,dz;

  m = 0;
  if (pbc_flag == 0) {
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = x[j][0];
      buf[m++] = x[j][1];
      buf[m++] = x[j][2];
      buf[m++] = ubuf(tag[j]).d;
      buf[m++] = ubuf(type[j]).d;
      buf[m++] = ubuf(mask[j]).d;
      buf[m++] = radius[j];
      buf[m++] = rmass[j];
      buf[m++] = density[j]; 
//
      buf[m++] = Lwx[j][0];
      buf[m++] = Lwx[j][1];
      buf[m++] = Lwx[j][2];
      buf[m++] = Lwx2[j][0];
      buf[m++] = Lwx2[j][1];
      buf[m++] = Lwx2[j][2];
//
    }
  } else {
    if (domain->triclinic == 0) {
      dx = pbc[0]*domain->xprd;
      dy = pbc[1]*domain->yprd;
      dz = pbc[2]*domain->zprd;
    } else {
      dx = pbc[0];
      dy = pbc[1];
      dz = pbc[2];
    }
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = x[j][0] + dx;
      buf[m++] = x[j][1] + dy;
      buf[m++] = x[j][2] + dz;
      buf[m++] = ubuf(tag[j]).d;
      buf[m++] = ubuf(type[j]).d;
      buf[m++] = ubuf(mask[j]).d;
      buf[m++] = radius[j];
      buf[m++] = rmass[j];
      buf[m++] = density[j]; 
//
      buf[m++] = Lwx[j][0];
      buf[m++] = Lwx[j][1];
      buf[m++] = Lwx[j][2];
      buf[m++] = Lwx2[j][0];
      buf[m++] = Lwx2[j][1];
      buf[m++] = Lwx2[j][2];
//
    }
  }

  if (atom->nextra_border)
    for (int iextra = 0; iextra < atom->nextra_border; iextra++)
      m += modify->fix[atom->extra_border[iextra]]->pack_border(n,list,&buf[m]);

  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::pack_border_vel(int n, int *list, double *buf,
                                     int pbc_flag, int *pbc)
{
  if(dynamic_cast<DomainWedge*>(domain))
    return pack_border_vel_wedge(n,list,buf,pbc_flag,pbc);

  int i,j,m;
  double dx,dy,dz,dvx,dvy,dvz;

  m = 0;
  if (pbc_flag == 0) {
    for (i = 0; i < n; i++) {
      j = list[i];
      buf[m++] = x[j][0];
      buf[m++] = x[j][1];
      buf[m++] = x[j][2];
      buf[m++] = ubuf(tag[j]).d;
      buf[m++] = ubuf(type[j]).d;
      buf[m++] = ubuf(mask[j]).d;
      buf[m++] = radius[j];
      buf[m++] = rmass[j];
      buf[m++] = density[j]; 
//
      buf[m++] = Lwx[j][0];
      buf[m++] = Lwx[j][1];
      buf[m++] = Lwx[j][2];
      buf[m++] = Lwx2[j][0];
      buf[m++] = Lwx2[j][1];
      buf[m++] = Lwx2[j][2];
//
      buf[m++] = v[j][0];
      buf[m++] = v[j][1];
      buf[m++] = v[j][2];
      buf[m++] = omega[j][0];
      buf[m++] = omega[j][1];
      buf[m++] = omega[j][2];
    }
  } else {
    if (domain->triclinic == 0) {
      dx = pbc[0]*domain->xprd;
      dy = pbc[1]*domain->yprd;
      dz = pbc[2]*domain->zprd;
    } else {
      dx = pbc[0];
      dy = pbc[1];
      dz = pbc[2];
    }
    if (!deform_vremap) {
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0] + dx;
        buf[m++] = x[j][1] + dy;
        buf[m++] = x[j][2] + dz;
        buf[m++] = ubuf(tag[j]).d;
        buf[m++] = ubuf(type[j]).d;
        buf[m++] = ubuf(mask[j]).d;
        buf[m++] = radius[j];
        buf[m++] = rmass[j];
        buf[m++] = density[j]; 
//
        buf[m++] = Lwx[j][0];
        buf[m++] = Lwx[j][1];
        buf[m++] = Lwx[j][2];
        buf[m++] = Lwx2[j][0];
        buf[m++] = Lwx2[j][1];
        buf[m++] = Lwx2[j][2];
//
        buf[m++] = v[j][0];
        buf[m++] = v[j][1];
        buf[m++] = v[j][2];
        buf[m++] = omega[j][0];
        buf[m++] = omega[j][1];
        buf[m++] = omega[j][2];
      }
    } else {
      dvx = pbc[0]*h_rate[0] + pbc[5]*h_rate[5] + pbc[4]*h_rate[4];
      dvy = pbc[1]*h_rate[1] + pbc[3]*h_rate[3];
      dvz = pbc[2]*h_rate[2];
      for (i = 0; i < n; i++) {
        j = list[i];
        buf[m++] = x[j][0] + dx;
        buf[m++] = x[j][1] + dy;
        buf[m++] = x[j][2] + dz;
        buf[m++] = ubuf(tag[j]).d;
        buf[m++] = ubuf(type[j]).d;
        buf[m++] = ubuf(mask[j]).d;
        buf[m++] = radius[j];
        buf[m++] = rmass[j];
        buf[m++] = density[j]; 
//
        buf[m++] = Lwx[j][0];
        buf[m++] = Lwx[j][1];
        buf[m++] = Lwx[j][2];
        buf[m++] = Lwx2[j][0];
        buf[m++] = Lwx2[j][1];
        buf[m++] = Lwx2[j][2];
//
        if (mask[i] & deform_groupbit) {
          buf[m++] = v[j][0] + dvx;
          buf[m++] = v[j][1] + dvy;
          buf[m++] = v[j][2] + dvz;
        } else {
          buf[m++] = v[j][0];
          buf[m++] = v[j][1];
          buf[m++] = v[j][2];
        }
        buf[m++] = omega[j][0];
        buf[m++] = omega[j][1];
        buf[m++] = omega[j][2];
      }
    }
  }

  if (atom->nextra_border)
    for (int iextra = 0; iextra < atom->nextra_border; iextra++)
      m += modify->fix[atom->extra_border[iextra]]->pack_border(n,list,&buf[m]);

  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::pack_border_hybrid(int n, int *list, double *buf)
{
  int i,j,m;

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];
    buf[m++] = radius[j];
    buf[m++] = rmass[j];
    buf[m++] = density[j]; 
//
    buf[m++] = Lwx[j][0];
    buf[m++] = Lwx[j][1];
    buf[m++] = Lwx[j][2];
    buf[m++] = Lwx2[j][0];
    buf[m++] = Lwx2[j][1];
    buf[m++] = Lwx2[j][2];
//
  }
  return m;
}

/* ---------------------------------------------------------------------- */

void AtomVecSphere::unpack_border(int n, int first, double *buf)
{
  int i,m,last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    if (i == nmax) grow(0);
    x[i][0] = buf[m++];
    x[i][1] = buf[m++];
    x[i][2] = buf[m++];
    tag[i] = (int) ubuf(buf[m++]).i;
    type[i] = (int) ubuf(buf[m++]).i;
    mask[i] = (int) ubuf(buf[m++]).i;
    radius[i] = buf[m++];
    rmass[i] = buf[m++];
    density[i] = buf[m++]; 
//
    Lwx[i][0] = buf[m++];
    Lwx[i][1] = buf[m++];
    Lwx[i][2] = buf[m++];
    Lwx2[i][0] = buf[m++];
    Lwx2[i][1] = buf[m++];
    Lwx2[i][2] = buf[m++];
//
  }

  if (atom->nextra_border)
    for (int iextra = 0; iextra < atom->nextra_border; iextra++)
      m += modify->fix[atom->extra_border[iextra]]->
        unpack_border(n,first,&buf[m]);
}

/* ---------------------------------------------------------------------- */

void AtomVecSphere::unpack_border_vel(int n, int first, double *buf)
{
  int i,m,last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    if (i == nmax) grow(0);
    x[i][0] = buf[m++];
    x[i][1] = buf[m++];
    x[i][2] = buf[m++];
    tag[i] = (int) ubuf(buf[m++]).i;
    type[i] = (int) ubuf(buf[m++]).i;
    mask[i] = (int) ubuf(buf[m++]).i;
    radius[i] = buf[m++];
    rmass[i] = buf[m++];
    density[i] = buf[m++]; 
//
    Lwx[i][0] = buf[m++];
    Lwx[i][1] = buf[m++];
    Lwx[i][2] = buf[m++];
    Lwx2[i][0] = buf[m++];
    Lwx2[i][1] = buf[m++];
    Lwx2[i][2] = buf[m++];
//
    v[i][0] = buf[m++];
    v[i][1] = buf[m++];
    v[i][2] = buf[m++];
    omega[i][0] = buf[m++];
    omega[i][1] = buf[m++];
    omega[i][2] = buf[m++];
    
  }
  if (atom->nextra_border)
    for (int iextra = 0; iextra < atom->nextra_border; iextra++)
      m += modify->fix[atom->extra_border[iextra]]->
        unpack_border(n,first,&buf[m]);
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::unpack_border_hybrid(int n, int first, double *buf)
{
  int i,m,last;

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    radius[i] = buf[m++];
    rmass[i] = buf[m++];
    density[i] = buf[m++]; 
//
    Lwx[i][0] = buf[m++];
    Lwx[i][1] = buf[m++];
    Lwx[i][2] = buf[m++];
    Lwx2[i][0] = buf[m++];
    Lwx2[i][1] = buf[m++];
    Lwx2[i][2] = buf[m++];
//
  }
  return m;
}

/* ----------------------------------------------------------------------
   pack data for atom I for sending to another proc
   xyz must be 1st 3 values, so comm::exchange() can test on them
------------------------------------------------------------------------- */

int AtomVecSphere::pack_exchange(int i, double *buf)
{
  int m = 1;
  buf[m++] = x[i][0];
  buf[m++] = x[i][1];
  buf[m++] = x[i][2];
  buf[m++] = v[i][0];
  buf[m++] = v[i][1];
  buf[m++] = v[i][2];
  buf[m++] = ubuf(tag[i]).d;
  buf[m++] = ubuf(type[i]).d;
  buf[m++] = ubuf(mask[i]).d;
  buf[m++] = ubuf(image[i]).d;

  buf[m++] = radius[i];
  buf[m++] = rmass[i];
  buf[m++] = density[i]; 
//
  buf[m++] = Lwx[i][0];
  buf[m++] = Lwx[i][1];
  buf[m++] = Lwx[i][2];
  buf[m++] = Lwx2[i][0];
  buf[m++] = Lwx2[i][1];
  buf[m++] = Lwx2[i][2];
//
  buf[m++] = omega[i][0];
  buf[m++] = omega[i][1];
  buf[m++] = omega[i][2];

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      m += modify->fix[atom->extra_grow[iextra]]->pack_exchange(i,&buf[m]);

  buf[0] = m;
  return m;
}

/* ---------------------------------------------------------------------- */

int AtomVecSphere::unpack_exchange(double *buf)
{
  int nlocal = atom->nlocal;
  if (nlocal == nmax) grow(0);

  int m = 1;
  x[nlocal][0] = buf[m++];
  x[nlocal][1] = buf[m++];
  x[nlocal][2] = buf[m++];
  v[nlocal][0] = buf[m++];
  v[nlocal][1] = buf[m++];
  v[nlocal][2] = buf[m++];
  tag[nlocal] = (int) ubuf(buf[m++]).i;
  type[nlocal] = (int) ubuf(buf[m++]).i;
  mask[nlocal] = (int) ubuf(buf[m++]).i;
  image[nlocal] = (tagint) ubuf(buf[m++]).i;

  radius[nlocal] = buf[m++];
  rmass[nlocal] = buf[m++];
  density[nlocal] = buf[m++];
//
  Lwx[nlocal][0] = buf[m++];
  Lwx[nlocal][1] = buf[m++];
  Lwx[nlocal][2] = buf[m++];
  Lwx2[nlocal][0] = buf[m++];
  Lwx2[nlocal][1] = buf[m++];
  Lwx2[nlocal][2] = buf[m++];
//
  omega[nlocal][0] = buf[m++];
  omega[nlocal][1] = buf[m++];
  omega[nlocal][2] = buf[m++];

  if (atom->nextra_grow)
    for (int iextra = 0; iextra < atom->nextra_grow; iextra++)
      m += modify->fix[atom->extra_grow[iextra]]->
        unpack_exchange(nlocal,&buf[m]);

  atom->nlocal++;
  return m;
}

/* ----------------------------------------------------------------------
   size of restart data for all atoms owned by this proc
   include extra data stored by fixes
------------------------------------------------------------------------- */

int AtomVecSphere::size_restart()
{
  int i;

  int nlocal = atom->nlocal;

//Lwx shuzixiugai
  int n = 23 * nlocal;  //20;17 * nlocal;
//

  if (atom->nextra_restart)
    for (int iextra = 0; iextra < atom->nextra_restart; iextra++)
      for (i = 0; i < nlocal; i++)
        n += modify->fix[atom->extra_restart[iextra]]->size_restart(i);

  return n;
}

/* ----------------------------------------------------------------------
   pack atom I's data for restart file including extra quantities
   xyz must be 1st 3 values, so that read_restart can test on them
   molecular types may be negative, but write as positive
------------------------------------------------------------------------- */

int AtomVecSphere::pack_restart(int i, double *buf)
{
  int m = 1;
  buf[m++] = x[i][0];
  buf[m++] = x[i][1];
  buf[m++] = x[i][2];
  buf[m++] = ubuf(tag[i]).d;
  buf[m++] = ubuf(type[i]).d;
  buf[m++] = ubuf(mask[i]).d;
  buf[m++] = ubuf(image[i]).d;
  
  buf[m++] = v[i][0];
  buf[m++] = v[i][1];
  buf[m++] = v[i][2];

  buf[m++] = radius[i];
  buf[m++] = rmass[i];
  buf[m++] = density[i]; 
//
  buf[m++] = Lwx[i][0];
  buf[m++] = Lwx[i][1];
  buf[m++] = Lwx[i][2];
  buf[m++] = Lwx2[i][0];
  buf[m++] = Lwx2[i][1];
  buf[m++] = Lwx2[i][2];
//
  buf[m++] = omega[i][0];
  buf[m++] = omega[i][1];
  buf[m++] = omega[i][2];

  if (atom->nextra_restart)
    for (int iextra = 0; iextra < atom->nextra_restart; iextra++)
      m += modify->fix[atom->extra_restart[iextra]]->pack_restart(i,&buf[m]);

  buf[0] = m;

  return m;
}

/* ----------------------------------------------------------------------
   unpack data for one atom from restart file including extra quantities
------------------------------------------------------------------------- */

int AtomVecSphere::unpack_restart(double *buf)
{
  int nlocal = atom->nlocal;
  if (nlocal == nmax) {
    grow(0);
    if (atom->nextra_store)
      memory->grow(atom->extra,nmax,atom->nextra_store,"atom:extra");
  }

  int m = 1;
  x[nlocal][0] = buf[m++];
  x[nlocal][1] = buf[m++];
  x[nlocal][2] = buf[m++];
  tag[nlocal] = (int) ubuf(buf[m++]).i;
  type[nlocal] = (int) ubuf(buf[m++]).i;
  mask[nlocal] = (int) ubuf(buf[m++]).i;
  image[nlocal] = (tagint) ubuf(buf[m++]).i;
  
  v[nlocal][0] = buf[m++];
  v[nlocal][1] = buf[m++];
  v[nlocal][2] = buf[m++];

  radius[nlocal] = buf[m++];
  rmass[nlocal] = buf[m++];
  density[nlocal] = buf[m++]; 
//
  Lwx[nlocal][0] = buf[m++];
  Lwx[nlocal][1] = buf[m++];
  Lwx[nlocal][2] = buf[m++];
  Lwx2[nlocal][0] = buf[m++];
  Lwx2[nlocal][1] = buf[m++];
  Lwx2[nlocal][2] = buf[m++];
//
  omega[nlocal][0] = buf[m++];
  omega[nlocal][1] = buf[m++];
  omega[nlocal][2] = buf[m++];

  double **extra = atom->extra;
  if (atom->nextra_store) {
    int size = static_cast<int> (buf[0]) - m;
    
    for (int i = 0; i < size; i++) extra[nlocal][i] = buf[m++];
  }

  atom->nlocal++;

  return m;
}

/* ----------------------------------------------------------------------
   create one atom of itype at coord
   set other values to defaults
------------------------------------------------------------------------- */

void AtomVecSphere::create_atom(int itype, double *coord)
{
  int nlocal = atom->nlocal;
  if (nlocal == nmax) grow(0);

  tag[nlocal] = 0;
  type[nlocal] = itype;
  x[nlocal][0] = coord[0];
  x[nlocal][1] = coord[1];
  x[nlocal][2] = coord[2];
  mask[nlocal] = 1;
  image[nlocal] = ((tagint) IMGMAX << IMG2BITS) |
    ((tagint) IMGMAX << IMGBITS) | IMGMAX;
  v[nlocal][0] = 0.0;
  v[nlocal][1] = 0.0;
  v[nlocal][2] = 0.0;

  radius[nlocal] = 0.5;
  density[nlocal] = 1.0;
//
  Lwx[nlocal][0] = 1000.0; //rhof
  Lwx[nlocal][1] = 1e-6; //nuf
  Lwx[nlocal][2] = 0.0; //conHap
  Lwx2[nlocal][0] = 1e-3; //alphas
  Lwx2[nlocal][1] = 0.0; //
  Lwx2[nlocal][2] = 0.0; //
//
  if (domain->dimension == 2) rmass[nlocal] = MY_PI *
      radius[nlocal]*radius[nlocal]*density[nlocal]; 
  else rmass[nlocal] = 4.0*MY_PI/3.0 *
    radius[nlocal]*radius[nlocal]*radius[nlocal] * density[nlocal]; 
  omega[nlocal][0] = 0.0;
  omega[nlocal][1] = 0.0;
  omega[nlocal][2] = 0.0;

  atom->nlocal++;
}

/* ----------------------------------------------------------------------
   unpack one line from Atoms section of data file
   initialize other atom quantities
------------------------------------------------------------------------- */

void AtomVecSphere::data_atom(double *coord, tagint imagetmp, char **values)
{
  int nlocal = atom->nlocal;
  if (nlocal == nmax) grow(0);

  tag[nlocal] = atoi(values[0]);
  if (tag[nlocal] <= 0)
    error->one(FLERR,"Invalid atom ID in Atoms section of data file");

  type[nlocal] = atoi(values[1]);
  if (type[nlocal] <= 0 || type[nlocal] > atom->ntypes)
    error->one(FLERR,"Invalid atom type in Atoms section of data file");

  radius[nlocal] = 0.5 * atof(values[2]);
  if (radius[nlocal] < 0.0)
    error->one(FLERR,"Invalid radius in Atoms section of data file");

  density[nlocal] = atof(values[3]);
  if (density[nlocal] <= 0.0)
    error->one(FLERR,"Invalid density in Atoms section of data file");

//
  Lwx[nlocal][0] = atof(values[4]);
  if (Lwx[nlocal][0] <= 0.0)
      error->one(FLERR, "Invalid Lwx[0] in Atoms section of data file");
  Lwx[nlocal][1] = atof(values[5]);
  if (Lwx[nlocal][1] <= 0.0)
      error->one(FLERR, "Invalid Lwx[1] in Atoms section of data file");
  Lwx[nlocal][2] = atof(values[6]);
  if (Lwx[nlocal][2] <= 0.0)
      error->one(FLERR, "Invalid Lwx[2] in Atoms section of data file");

  Lwx2[nlocal][0] = atof(values[7]);
  if (Lwx2[nlocal][0] <= 0.0)
      error->one(FLERR, "Invalid Lwx2[0] in Atoms section of data file");
  Lwx2[nlocal][1] = atof(values[8]);
  if (Lwx2[nlocal][1] <= 0.0)
      error->one(FLERR, "Invalid Lwx2[1] in Atoms section of data file");
  Lwx2[nlocal][2] = atof(values[9]);
  if (Lwx2[nlocal][2] <= 0.0)
      error->one(FLERR, "Invalid Lwx2[2] in Atoms section of data file");
//

  if (radius[nlocal] == 0.0) rmass[nlocal] = density[nlocal];
  else
  {
    if (domain->dimension == 2) rmass[nlocal] = MY_PI *
      radius[nlocal]*radius[nlocal]*density[nlocal]; 
    else rmass[nlocal] = 4.0*MY_PI/3.0 *
      radius[nlocal]*radius[nlocal]*radius[nlocal] * density[nlocal];
  }

  x[nlocal][0] = coord[0];
  x[nlocal][1] = coord[1];
  x[nlocal][2] = coord[2];

  image[nlocal] = imagetmp;

  mask[nlocal] = 1;
  v[nlocal][0] = 0.0;
  v[nlocal][1] = 0.0;
  v[nlocal][2] = 0.0;
  omega[nlocal][0] = 0.0;
  omega[nlocal][1] = 0.0;
  omega[nlocal][2] = 0.0;

  atom->nlocal++;
}

/* ----------------------------------------------------------------------
   unpack hybrid quantities from one line in Atoms section of data file
   initialize other atom quantities for this sub-style
------------------------------------------------------------------------- */

int AtomVecSphere::data_atom_hybrid(int nlocal, char **values)
{
  radius[nlocal] = 0.5 * atof(values[0]);
  if (radius[nlocal] < 0.0)
    error->one(FLERR,"Invalid radius in Atoms section of data file");

  density[nlocal] = atof(values[1]);
  if (density[nlocal] <= 0.0)
    error->one(FLERR,"Invalid density in Atoms section of data file");

/*
  Lwx[nlocal] = atof(values[2]);
 if (Lwx[nlocal] <= 0.0)
   error->one(FLERR,"Invalid Lwx in Atoms section of data file");

  Lwx2[nlocal] = atof(values[3]);
 if (Lwx2[nlocal] <= 0.0)
   error->one(FLERR,"Invalid Lwx2 in Atoms section of data file");
*/

  if (radius[nlocal] == 0.0) rmass[nlocal] = density[nlocal];
  else
    rmass[nlocal] = 4.0*MY_PI/3.0 *
      radius[nlocal]*radius[nlocal]*radius[nlocal] * density[nlocal];

  return 2;
}

/* ----------------------------------------------------------------------
   unpack one line from Velocities section of data file
------------------------------------------------------------------------- */

void AtomVecSphere::data_vel(int m, char **values)
{
  v[m][0] = atof(values[0]);
  v[m][1] = atof(values[1]);
  v[m][2] = atof(values[2]);
  omega[m][0] = atof(values[3]);
  omega[m][1] = atof(values[4]);
  omega[m][2] = atof(values[5]);
}

/* ----------------------------------------------------------------------
   unpack hybrid quantities from one line in Velocities section of data file
------------------------------------------------------------------------- */

int AtomVecSphere::data_vel_hybrid(int m, char **values)
{
  omega[m][0] = atof(values[0]);
  omega[m][1] = atof(values[1]);
  omega[m][2] = atof(values[2]);
  return 3;
}

/* ----------------------------------------------------------------------
   pack atom info for data file including 3 image flags
------------------------------------------------------------------------- */

void AtomVecSphere::pack_data(double **buf)
{
  int nlocal = atom->nlocal;
  for (int i = 0; i < nlocal; i++) {
    buf[i][0] = ubuf(tag[i]).d;
    buf[i][1] = ubuf(type[i]).d;
    buf[i][2] = 2.0*radius[i];
    if (radius[i] == 0.0) buf[i][3] = rmass[i];
    else
      buf[i][3] = rmass[i] / (4.0*MY_PI/3.0 * radius[i]*radius[i]*radius[i]);
    buf[i][4] = x[i][0];
    buf[i][5] = x[i][1];
    buf[i][6] = x[i][2];
    buf[i][7] = ubuf((image[i] & IMGMASK) - IMGMAX).d;
    buf[i][8] = ubuf((image[i] >> IMGBITS & IMGMASK) - IMGMAX).d;
    buf[i][9] = ubuf((image[i] >> IMG2BITS) - IMGMAX).d;
//
    buf[i][10] = Lwx[i][0];
    buf[i][11] = Lwx[i][1];
    buf[i][12] = Lwx[i][2];
    buf[i][13] = Lwx2[i][0];
    buf[i][14] = Lwx2[i][1];
    buf[i][15] = Lwx2[i][2];
//
  }
}

/* ----------------------------------------------------------------------
   pack atom info for data file including 3 image flags
------------------------------------------------------------------------- */

void AtomVecSphere::pack_data(double **buf,int tag_offset)
{
  int nlocal = atom->nlocal;
  for (int i = 0; i < nlocal; i++) {
    buf[i][0] = ubuf(tag[i]+tag_offset).d; 
    buf[i][1] = ubuf(type[i]).d;
    buf[i][2] = 2.0*radius[i];
    if (radius[i] == 0.0) buf[i][3] = rmass[i];
    else
      buf[i][3] = rmass[i] / (4.0*MY_PI/3.0 * radius[i]*radius[i]*radius[i]);
    buf[i][4] = x[i][0];
    buf[i][5] = x[i][1];
    buf[i][6] = x[i][2];
    buf[i][7] = ubuf((image[i] & IMGMASK) - IMGMAX).d;
    buf[i][8] = ubuf((image[i] >> IMGBITS & IMGMASK) - IMGMAX).d;
    buf[i][9] = ubuf((image[i] >> IMG2BITS) - IMGMAX).d;
//
    buf[i][10] = Lwx[i][0];
    buf[i][11] = Lwx[i][1];
    buf[i][12] = Lwx[i][2];
    buf[i][13] = Lwx2[i][0];
    buf[i][14] = Lwx2[i][1];
    buf[i][15] = Lwx2[i][2];
//
  }
}

/* ----------------------------------------------------------------------
   pack hybrid atom info for data file
------------------------------------------------------------------------- */

int AtomVecSphere::pack_data_hybrid(int i, double *buf)
{
  buf[0] = 2.0*radius[i];
//
  //buf[1] = Lwx[i];
  
  //buf[2] = Lwx2[i];
//
  if (radius[i] == 0.0) buf[1] = rmass[i];
  else buf[1] = rmass[i] / (4.0*MY_PI/3.0 * radius[i]*radius[i]*radius[i]);
  return 2;
}

/* ----------------------------------------------------------------------
   write atom info to data file including 3 image flags
------------------------------------------------------------------------- */

void AtomVecSphere::write_data(FILE *fp, int n, double **buf)
{
  for (int i = 0; i < n; i++)
    fprintf(fp,"%d %d %-1.16e %-1.16e %-1.16e %-1.16e %-1.16e %d %d %d\n",
            (int) ubuf(buf[i][0]).i,(int) ubuf(buf[i][1]).i,
            buf[i][2],buf[i][3],
            buf[i][4],buf[i][5],buf[i][6],
            (int) ubuf(buf[i][7]).i,(int) ubuf(buf[i][8]).i,
            (int) ubuf(buf[i][9]).i);
}

/* ----------------------------------------------------------------------
   write hybrid atom info to data file
------------------------------------------------------------------------- */

int AtomVecSphere::write_data_hybrid(FILE *fp, double *buf)
{
  fprintf(fp," %-1.16e %-1.16e",buf[0],buf[1]);
  return 2;
}

/* ----------------------------------------------------------------------
   pack velocity info for data file
------------------------------------------------------------------------- */

void AtomVecSphere::pack_vel(double **buf)
{
  int nlocal = atom->nlocal;
  for (int i = 0; i < nlocal; i++) {
    buf[i][0] = ubuf(tag[i]).d;
    buf[i][1] = v[i][0];
    buf[i][2] = v[i][1];
    buf[i][3] = v[i][2];
    buf[i][4] = omega[i][0];
    buf[i][5] = omega[i][1];
    buf[i][6] = omega[i][2];
  }
}

/* ----------------------------------------------------------------------
   pack velocity info for data file
------------------------------------------------------------------------- */

void AtomVecSphere::pack_vel(double **buf,int tag_offset) 
{
  int nlocal = atom->nlocal;
  for (int i = 0; i < nlocal; i++) {
    buf[i][0] = ubuf(tag[i]+tag_offset).d;
    buf[i][1] = v[i][0];
    buf[i][2] = v[i][1];
    buf[i][3] = v[i][2];
    buf[i][4] = omega[i][0];
    buf[i][5] = omega[i][1];
    buf[i][6] = omega[i][2];
  }
}

/* ----------------------------------------------------------------------
   pack hybrid velocity info for data file
------------------------------------------------------------------------- */

int AtomVecSphere::pack_vel_hybrid(int i, double *buf)
{
  buf[0] = omega[i][0];
  buf[1] = omega[i][1];
  buf[2] = omega[i][2];
  return 3;
}

/* ----------------------------------------------------------------------
   write velocity info to data file
------------------------------------------------------------------------- */

void AtomVecSphere::write_vel(FILE *fp, int n, double **buf)
{
  for (int i = 0; i < n; i++)
    fprintf(fp,"%d %-1.16e %-1.16e %-1.16e %-1.16e %-1.16e %-1.16e\n",
            (int) ubuf(buf[i][0]).i,buf[i][1],buf[i][2],buf[i][3],
            buf[i][4],buf[i][5],buf[i][6]);
}

/* ----------------------------------------------------------------------
   write hybrid velocity info to data file
------------------------------------------------------------------------- */

int AtomVecSphere::write_vel_hybrid(FILE *fp, double *buf)
{
  fprintf(fp," %-1.16e %-1.16e %-1.16e",buf[0],buf[1],buf[2]);
  return 3;
}

/* ----------------------------------------------------------------------
   return # of bytes of allocated memory
------------------------------------------------------------------------- */

bigint AtomVecSphere::memory_usage()
{
  bigint bytes = 0;

  if (atom->memcheck("tag")) bytes += memory->usage(tag,nmax);
  if (atom->memcheck("type")) bytes += memory->usage(type,nmax);
  if (atom->memcheck("mask")) bytes += memory->usage(mask,nmax);
  if (atom->memcheck("image")) bytes += memory->usage(image,nmax);
  if (atom->memcheck("x")) bytes += memory->usage(x,nmax,3);
  if (atom->memcheck("v")) bytes += memory->usage(v,nmax,3);
  if (atom->memcheck("f")) bytes += memory->usage(f,nmax*comm->nthreads,3);

  if (atom->memcheck("density")) bytes += memory->usage(density,nmax);
//
  if (atom->memcheck("Lwx")) bytes += memory->usage(Lwx, nmax, 3);
  if (atom->memcheck("Lwx2")) bytes += memory->usage(Lwx2, nmax, 3);
//
  if (atom->memcheck("radius")) bytes += memory->usage(radius,nmax);
  if (atom->memcheck("rmass")) bytes += memory->usage(rmass,nmax);
  if (atom->memcheck("omega")) bytes += memory->usage(omega,nmax,3);
  if (atom->memcheck("torque"))
    bytes += memory->usage(torque,nmax*comm->nthreads,3);

  return bytes;
}
