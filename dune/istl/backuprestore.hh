// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifndef DUNE_ISTL_BACKUPRESTORE_HH
#define DUNE_ISTL_BACKUPRESTORE_HH

#include<fstream>

#include<dune/common/parallel/mpihelper.hh>
#include<dune/common/fvector.hh>
#include<dune/istl/bvector.hh>


namespace Dune {

  template<class DATA>
  struct ISTLBackupRestoreHelper
  {
    static void backup(const DATA& data, std::ofstream& stream)
    {
      stream.write(reinterpret_cast<const char*>(&data), sizeof(DATA));
    }

    static void restore(DATA& data, std::ifstream& stream)
    {
      stream.read(reinterpret_cast<char*>(&data), sizeof(DATA));
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
    template<class Comm = Dune::MPIHelper::MPICommunicator>
    static void backup(const V& vector, const std::string& filename,
                       typename Dune::CollectiveCommunication<Comm> comm = Dune::CollectiveCommunication<Comm>())
    {
      std::ostringstream filename_str;
      filename_str << filename;
      if (comm.size() > 0)
        filename_str << comm.rank();
      std::ofstream file;
      file.open(filename_str.str(), std::ios::out | std::ios::binary);
      if (file)
        ISTLBackupRestoreHelper<V>::backup(vector,file);
      else
        DUNE_THROW(Dune::Exception, "Could not open file to backup vector!");
      file.close();
    }

    template<class Comm = Dune::MPIHelper::MPICommunicator>
    static void restore(V& vector, const std::string& filename, typename
                        Dune::CollectiveCommunication<Comm> comm = Dune::CollectiveCommunication<Comm>())
    {
      std::ostringstream filename_str;
      filename_str << filename;
      if (comm.size() > 0)
        filename_str << comm.rank();
      std::ifstream file;
      file.open(filename_str.str(), std::ios::in | std::ios::binary);
      if (file)
        ISTLBackupRestoreHelper<V>::restore(vector,file);
      else
        DUNE_THROW(Dune::Exception, "Could not open file to restore vector!");
      file.close();
    }
  };

}

#endif
