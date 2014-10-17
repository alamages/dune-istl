// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifndef DUNE_ISTL_BACKUPRESTORE_HH
#define DUNE_ISTL_BACKUPRESTORE_HH

#include<fstream>

#include<dune/common/parallel/mpihelper.hh>
#include<dune/common/fvector.hh>
#include<dune/istl/bvector.hh>

// TODO list
// - find a way to properly pass the rank to facility
// - evaluate whether this needs to be MPI_COMM_WORLD (see yasp)
// - switch to binary mode
// - test the restoring. it does not work until binary mode is on or separators are introduced.


namespace Dune {

  template<class DATA>
  struct ISTLBackupRestoreHelper
  {
    static void backup(const DATA& data, std::ofstream& stream)
    {
      stream << data;
    }

    static void restore(DATA& data, std::ifstream& stream)
    {
      stream >> data;
    }
  };

  template<class Block, class Allocator>
  struct ISTLBackupRestoreHelper<Dune::BlockVector<Block, Allocator> >
  {
    typedef typename Dune::BlockVector<Block, Allocator> Data;
    typedef typename Data::size_type size_type;

    static void backup(const Data& data, std::ofstream& stream)
    {
      stream << data.size();
      for (size_type i=0; i<data.size(); i++)
        ISTLBackupRestoreHelper<Block>::backup(data[i],stream);
    }

    static void restore(Data& data, std::ifstream& stream)
    {
      size_type vsize;
      stream >> vsize;
      data.resize(vsize);
      for (size_type i = 0; i<vsize; i++)
        ISTLBackupRestoreHelper<Block>::restore(data[i], stream);
    }
  };

  template<class T, int dim>
  struct ISTLBackupRestoreHelper<Dune::FieldVector<T,dim> >
  {
    typedef typename Dune::FieldVector<T,dim> Data;
    typedef typename Data::size_type size_type;

    static void backup(const Data& data, std::ofstream& stream)
    {
      for (size_type i=0; i<dim; i++)
        ISTLBackupRestoreHelper<T>::backup(data[i], stream);
    }

    static void restore(Data& data, std::ifstream& stream)
    {
      for (size_type i=0; i<dim; ++i)
        ISTLBackupRestoreHelper<T>::restore(data[i], stream);
    }
  };

  template<class V>
  struct ISTLBackupRestoreFacility
  {
    template<class MPI>
    static void backup(const V& vector, const std::string& filename, const MPI& mpi = Dune::FakeMPIHelper::instance(0,NULL))
    {
      std::ostringstream filename_str;
      filename_str << filename;
      if (!MPI::isFake)
        filename_str << mpi.rank();
      std::ofstream file;
      file.open(filename_str.str());
      if (file)
        ISTLBackupRestoreHelper<V>::backup(vector,file);
      else
        DUNE_THROW(Dune::Exception, "Could not open file to backup vector!");
      file.close();
    }

    template<class MPI>
    static void restore(V& vector, std::string filename, const MPI& mpi = Dune::FakeMPIHelper::instance(0,NULL))
    {
      std::ostringstream filename_str;
      filename_str << filename;
      if (!MPI::isFake)
        filename_str << mpi.rank();
      std::ifstream file;
      file.open(filename_str.str());
      if (file)
        ISTLBackupRestoreHelper<V>::restore(vector,file);
      else
        DUNE_THROW(Dune::Exception, "Could not open file to restore vector!");
      file.close();
    }
  };

}

#endif
