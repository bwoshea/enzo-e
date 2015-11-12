// See LICENSE_CELLO file for license and copyright information

/// @file     io_OutputData.cpp
/// @author   James Bordner (jobordner@ucsd.edu)
/// @date     Thu Mar 17 11:14:18 PDT 2011
/// @brief    Implementation of the OutputData class

#include "cello.hpp"
#include "io.hpp"

//----------------------------------------------------------------------

OutputData::OutputData
(
 int index,
 const Factory * factory,
 const FieldDescr * field_descr,
 const ParticleDescr * particle_descr,
 Config * config
) throw ()
  : Output(index,factory,field_descr,particle_descr)
{
  // Set process stride, with default = 1

  int stride = config->output_stride[index_];

  process_stride_ = stride == 0 ? 1 : stride;
  set_process_stride(process_stride_);
}

//----------------------------------------------------------------------

OutputData::~OutputData() throw()
{
  close();
}

//----------------------------------------------------------------------

void OutputData::pup (PUP::er &p)
{

  TRACEPUP;

  // NOTE: change this function whenever attributes change

  Output::pup(p);

  // this function intentionally left blank
}

//======================================================================

void OutputData::open () throw()
{
  std::string file_name = expand_file_name_(&file_name_,&file_args_);

  Monitor::instance()->print 
    ("Output","writing data file %s", file_name.c_str());

  close();

  file_ = new FileHdf5 (".",file_name);

  file_->file_create();
}

//----------------------------------------------------------------------

void OutputData::close () throw()
{
  if (file_) file_->file_close();
  delete file_;  file_ = 0;
}

//----------------------------------------------------------------------

void OutputData::finalize () throw ()
{  Output::finalize(); }

//----------------------------------------------------------------------

void OutputData::write_hierarchy
(
 const Hierarchy  * hierarchy,
 const FieldDescr * field_descr,
 const ParticleDescr * particle_descr
 ) throw()
{
  IoHierarchy io_hierarchy(hierarchy);

  write_meta (&io_hierarchy);

  Output::write_hierarchy(hierarchy, field_descr, particle_descr);

}

//----------------------------------------------------------------------

void OutputData::write_block
( 
  const Block * block,
  const FieldDescr * field_descr,
  const ParticleDescr * particle_descr) throw()
{

  // Create file group for block

  std::string group_name = "/" + block->name();

  DEBUG1 ("block name = %s",group_name.c_str());
  file_->group_chdir(group_name);
  file_->group_create();

  // Write block meta data

  io_block()->set_block((Block *)block);

  write_meta_group (io_block());

  // Call write(block) on base Output object

  Output::write_block(block,field_descr,particle_descr);

  file_->group_close();

}

//----------------------------------------------------------------------

void OutputData::write_field_data
( 
  const FieldData * field_data,
  const FieldDescr * field_descr,
  int index_field) throw()
{
  io_field_data()->set_field_descr((FieldDescr*)field_descr);
  io_field_data()->set_field_data((FieldData*)field_data);
  io_field_data()->set_field_index(index_field);

  for (size_t i=0; i<io_field_data()->data_count(); i++) {

    void * buffer;
    std::string name;
    int type;
    int nxd,nyd,nzd;  // Array dimension
    int nx,ny,nz;     // Array size

    // Get ith FieldData data
    io_field_data()->field_array(i, &buffer, &name, &type, 
				 &nxd,&nyd,&nzd,
				 &nx, &ny, &nz);

    // Write ith FieldData data

    file_->data_create(name.c_str(),type,nxd,nyd,nzd,nx,ny,nz);
    file_->data_write(buffer);
    file_->data_close();
  }

}

//----------------------------------------------------------------------

void OutputData::write_particle_data
( 
  const ParticleData * particle_data,
  const ParticleDescr * particle_descr,
  int it) throw()
{
  const Particle particle ( (ParticleDescr*) particle_descr,
			    (ParticleData*)  particle_data);

  // Write particle data for particle type it

  io_particle_data()->set_particle_descr ( (ParticleDescr*) particle_descr);
  io_particle_data()->set_particle_data  ( (ParticleData*)  particle_data);
  io_particle_data()->set_particle_index(it);

  // loop through attributes 
  // loop through blocks
  // write particle data for [it][ib][ia]

  const int nb = particle.num_batches(it);
  const int na = particle.num_attributes(it);

  for (int ia=0; ia<na; ia++) {
    for (int ib=0; ib<nb; ib++) {

      // write ith batch of particle index 
      void * buffer;
      std::string name;
      int type;
      int n,k;

      // Get ith batch of Particle data for particle type it
      io_particle_data()->particle_array(it,ib,ia,
				      &buffer, &name, &type, 
				      &n,&k);

      // Write ith batch of attributes

      int nxd,nyd,nzd;
      int nx,ny,nz;

      if (particle.interleaved(it)) {
	nxd=k;	nyd=n;	nzd=1;
	nx=1;	ny=n;	nz=1;
      } else {
	nxd=n;	nyd=1;	nzd=1;
	nx=n;	ny=1;	nz=1;
      }

      file_->data_create(name.c_str(),type,nxd,nyd,nzd,nx,ny,nz);
      file_->data_write(buffer);
      file_->data_close();
    }
  }

}

//======================================================================
