// See LICENSE_CELLO file for license and copyright information

/// @file     enzo_EnzoMethodPmDeposit.cpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     Fri Apr  2 17:05:23 PDT 2010
/// @brief    Implements the EnzoMethodPmDeposit class
///
/// The EnzoMethodPmDeposit method computes a "density_total" field,
/// which includes the "density" field plus mass from gravitating
/// particles (particles in the "mass" group, e.g. "dark" matter
/// particles)

#include "cello.hpp"
#include "enzo.hpp"

#define DEBUG_PM_DEPOSIT

#ifdef DEBUG_PM_DEPOSIT
#  define TRACE_PM(MESSAGE)				\
  CkPrintf ("%s:%d %s\n", __FILE__,__LINE__,MESSAGE);				
#else
#  define TRACE_PM(MESSAGE) /* ... */
#endif

//----------------------------------------------------------------------

EnzoMethodPmDeposit::EnzoMethodPmDeposit (const FieldDescr * field_descr,
					  std::string type)
  : Method(),
    type_(pm_type_unknown)
{
  TRACE_PM("EnzoMethodPmDeposit()");
  if (type == "cic") {
    type_ = pm_type_cic;
  } else if (type == "ngp") {
    type_ = pm_type_ngp;
    WARNING("EnzoMethodPmDeposit()",
	    "PM deposit type 'ngp' is not implemented yet: using 'cic'");
  } else if (type == "tsc") {
    WARNING("EnzoMethodPmDeposit()",
	    "PM deposit type 'tsc' is not implemented yet: using 'cic'");
    type_ = pm_type_tsc;
  } else {
    ERROR1 ("EnzoMethodPmDeposit()",
	    "PM deposit type '%s' is not supported: "
	    "must be 'cic','ngp', or 'tsc'",
	    type.c_str());
  }
  // Initialize default Refresh object

  const int ir = add_refresh(4,0,neighbor_leaf,sync_barrier);
  refresh(ir)->add_all_fields(field_descr->field_count());

  // PM parameters initialized in EnzoBlock::initialize()
}

//----------------------------------------------------------------------

void EnzoMethodPmDeposit::pup (PUP::er &p)
{
  // NOTE: change this function whenever attributes change

  TRACEPUP;

  Method::pup(p);

  p | type_;
}

//----------------------------------------------------------------------

void EnzoMethodPmDeposit::compute ( Block * block) throw()
{
  TRACE_PM("compute()");
  EnzoBlock * enzo_block = static_cast<EnzoBlock*> (block);

  if (block->is_leaf()) {

    Particle particle (block->data()->particle());
    Field    field    (block->data()->field());

    const int rank = block->rank();

    double  * de   = (double *) field.values("density");
    double  * de_t = (double *) field.values("density_total");

    Grouping * group = particle.groups();

    int mx,my,mz;
    field.dimensions(0,&mx,&my,&mz);
    int nx,ny,nz;
    field.size(&nx,&ny,&nz);
    int gx,gy,gz;
    field.ghost_depth(0,&gx,&gy,&gz);

    // Initialize "density_total" with gas "density"

    for (int iz=0; iz<mz; iz++) {
      for (int iy=0; iy<my; iy++) {
	for (int ix=0; ix<mx; ix++) {
	  int i = ix + mx*(iy + my*iz);
	  de_t[i]  = de[i];
	}
      }
    }

    // Get block extents and cell widths
    double xm,ym,zm;
    double xp,yp,zp;
    block->lower(&xm,&ym,&zm);
    block->upper(&xp,&yp,&zp);

    const double hx = (xp-xm)/nx;
    const double hy = (yp-ym)/ny;
    const double hz = (zp-zm)/nz;

    // declare particle position arrays

    const int nt = particle.num_types();

    const int it = particle.type_index ("dark");

    const int ia_mass = particle.constant_index (it,"mass");

    double mass = *((double *)(particle.constant_value (it,ia_mass)));

    const int ia_x = particle.attribute_index(it,"x");
    const int ia_y = particle.attribute_index(it,"y");
    const int ia_z = particle.attribute_index(it,"z");

    const int dp =  particle.stride(it,ia_x);

    // Accumulate particle density using CIC

    for (int ib=0; ib<particle.num_batches(it); ib++) {

      const int np = particle.num_particles(it,ib);

      double * ya = (double *) particle.attribute_array (it,ia_y,ib);
      double * za = (double *) particle.attribute_array (it,ia_z,ib);

      if (rank == 1) {

	double * xa = (double *) particle.attribute_array (it,ia_x,ib);

	for (int ip=0; ip<np; ip++) {

	  double x = xa[ip*dp];

	  double tx = nx*(x - xm) / (xp - xm) - 0.5;

	  int ix0 = gx + floor(tx);

	  int ix1 = ix0 + 1;

	  double x0 = 1.0 - (tx - floor(tx));

	  double x1 = 1.0 - x0;

	  de_t[ix0] += mass*x0;
	  de_t[ix1] += mass*x1;

	}
      } else if (rank == 2) {

	double * xa = (double *) particle.attribute_array (it,ia_x,ib);
	double * ya = (double *) particle.attribute_array (it,ia_y,ib);

	for (int ip=0; ip<np; ip++) {

	  double x = xa[ip*dp];
	  double y = ya[ip*dp];

	  double tx = nx*(x - xm) / (xp - xm) - 0.5;
	  double ty = ny*(y - ym) / (yp - ym) - 0.5;

	  int ix0 = gx + floor(tx);
	  int iy0 = gy + floor(ty);

	  int ix1 = ix0 + 1;
	  int iy1 = iy0 + 1;

	  double x0 = 1.0 - (tx - floor(tx));
	  double y0 = 1.0 - (ty - floor(ty));

	  double x1 = 1.0 - x0;
	  double y1 = 1.0 - y0;


	  de_t[ix0+mx*iy0] += mass*x0*y0;
	  de_t[ix1+mx*iy0] += mass*x1*y0;
	  de_t[ix0+mx*iy1] += mass*x0*y1;
	  de_t[ix1+mx*iy1] += mass*x1*y1;

	}

      } else if (rank == 3) {

	double * xa = (double *) particle.attribute_array (it,ia_x,ib);
	double * ya = (double *) particle.attribute_array (it,ia_y,ib);

	for (int ip=0; ip<np; ip++) {

	  double x = xa[ip*dp];
	  double y = ya[ip*dp];
	  double z = za[ip*dp];

	  double tx = nx*(x - xm) / (xp - xm) - 0.5;
	  double ty = ny*(y - ym) / (yp - ym) - 0.5;
	  double tz = nz*(z - zm) / (zp - zm) - 0.5;

	  int ix0 = gx + floor(tx);
	  int iy0 = gy + floor(ty);
	  int iz0 = gz + floor(tz);

	  int ix1 = ix0 + 1;
	  int iy1 = iy0 + 1;
	  int iz1 = iz0 + 1;

	  double x0 = 1.0 - (tx - floor(tx));
	  double y0 = 1.0 - (ty - floor(ty));
	  double z0 = 1.0 - (tz - floor(tz));

	  double x1 = 1.0 - x0;
	  double y1 = 1.0 - y0;
	  double z1 = 1.0 - z0;


	  de_t[ix0+mx*(iy0+my*iz0)] += mass*x0*y0*z0;
	  de_t[ix1+mx*(iy0+my*iz0)] += mass*x1*y0*z0;
	  de_t[ix0+mx*(iy1+my*iz0)] += mass*x0*y1*z0;
	  de_t[ix1+mx*(iy1+my*iz0)] += mass*x1*y1*z0;
	  de_t[ix0+mx*(iy0+my*iz1)] += mass*x0*y0*z1;
	  de_t[ix1+mx*(iy0+my*iz1)] += mass*x1*y0*z1;
	  de_t[ix0+mx*(iy1+my*iz1)] += mass*x0*y1*z1;
	  de_t[ix1+mx*(iy1+my*iz1)] += mass*x1*y1*z1;

	}
      }
    }
  }

  block->compute_done(); 
  
}

//----------------------------------------------------------------------

double EnzoMethodPmDeposit::timestep ( Block * block ) const throw()
{
  TRACE_PM("timestep()");
  double dt = std::numeric_limits<double>::max();

  return dt;
}
