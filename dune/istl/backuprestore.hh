// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifndef DUNE_ISTL_BACKUPRESTORE_HH
#define DUNE_ISTL_BACKUPRESTORE_HH

#include<fstream>

#include<dune/common/parallel/mpihelper.hh>
#include<dune/common/fvector.hh>
#include<dune/istl/bvector.hh>


namespace Dune {


  /** \brief update a fletcher64 checksum
   * \param data the pointer to the data
   * \param length the size of the memory block to calculate the checksum of
   * \param sum1 the first half of the checksum to start with
   * \param sum2 the second half of the checksum to start with
   *
   * No checksum is returned, but instead the variables sum1 and sum2 are updated to be
   * combined to a checksum as soon as we are ready. This is done because we dont want
   * to iterate over the data twice but instead update the checksum on the fly.
   */
  void fletcher64(const uint32_t* data, std::size_t length, uint64_t& sum1, uint64_t& sum2)
  {
    while (length >= 4)
    {
      // update the checksum
      sum1 += *data;
      sum1 &= 0xffffffff;
      sum2 += sum1;
      sum2 &= 0xffffffff;
      length -= 4;
      ++data;
    }
    if (length > 0)
    {
      // take 32 bits where only th leftmost length bits are part of the data
      uint32_t rest = *data;
      // create a mask having 1s on all bits that are not part of the data
      uint32_t mask = 0;
      for (int i=0; i < 32-length; i++)
      {
        mask += 1;
        mask <<= 1;
      }
      // switch all bits not in the data to 1
      rest |= mask;

      // update the checksum
      sum1 += rest;
      sum1 &= 0xffffffff;
      sum2 += sum1;
      sum2 &= 0xffffffff;
    }
  }

  template<class DATA>
  void writeToStreamWithChecksum(const DATA& data, std::ofstream& stream, uint64_t& sum1, uint64_t& sum2)
  {
    stream.write(reinterpret_cast<const char*>(&data), sizeof(DATA));
    fletcher64(reinterpret_cast<const uint32_t*>(&data), sizeof(DATA), sum1, sum2);
  }

  template<class DATA>
  void readFromStreamWithChecksum(DATA& data, std::ifstream& stream, uint64_t& sum1, uint64_t& sum2)
  {
    stream.read(reinterpret_cast<char*>(&data), sizeof(DATA));
    fletcher64(reinterpret_cast<const uint32_t*>(&data), sizeof(DATA), sum1, sum2);
  }

  template<class DATA>
  struct ISTLBackupRestoreHelper
  {
    static void backup(const DATA& data, std::ofstream& stream, uint64_t& sum1, uint64_t& sum2)
    {
      writeToStreamWithChecksum(data, stream, sum1, sum2);
    }

    static void restore(DATA& data, std::ifstream& stream, uint64_t& sum1, uint64_t& sum2)
    {
      readFromStreamWithChecksum(data, stream, sum1, sum2);
    }
  };

  template<class Block, class Allocator>
  struct ISTLBackupRestoreHelper<Dune::BlockVector<Block, Allocator> >
  {
    typedef typename Dune::BlockVector<Block, Allocator> Data;
    typedef typename Data::size_type size_type;

    static void backup(const Data& data, std::ofstream& stream, uint64_t& sum1, uint64_t& sum2)
    {
      size_type size = data.size();
      writeToStreamWithChecksum(size, stream, sum1, sum2);
      for (size_type i=0; i<data.size(); i++)
        ISTLBackupRestoreHelper<Block>::backup(data[i], stream, sum1, sum2);
    }

    static void restore(Data& data, std::ifstream& stream, uint64_t& sum1, uint64_t& sum2)
    {
      size_type size;
      readFromStreamWithChecksum(size, stream, sum1, sum2);
      data.resize(size);
      for (size_type i = 0; i<size; i++)
        ISTLBackupRestoreHelper<Block>::restore(data[i], stream, sum1, sum2);
    }
  };

  template<class T, int dim>
  struct ISTLBackupRestoreHelper<Dune::FieldVector<T,dim> >
  {
    typedef typename Dune::FieldVector<T,dim> Data;
    typedef typename Data::size_type size_type;

    static void backup(const Data& data, std::ofstream& stream, uint64_t& sum1, uint64_t& sum2)
    {
      for (size_type i=0; i<dim; i++)
        ISTLBackupRestoreHelper<T>::backup(data[i], stream, sum1, sum2);
    }

    static void restore(Data& data, std::ifstream& stream, uint64_t& sum1, uint64_t& sum2)
    {
      for (size_type i=0; i<dim; ++i)
        ISTLBackupRestoreHelper<T>::restore(data[i], stream, sum1, sum2);
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
      if (comm.size() > 1)
        filename_str << comm.rank();
      std::ofstream file;
      file.open(filename_str.str(), std::ios::out | std::ios::binary);
      if (file)
      {
        // initialize the two fletcher sums
        uint64_t sum1 = 0;
        uint64_t sum2 = 0;

        // do the actual backup
        ISTLBackupRestoreHelper<V>::backup(vector, file, sum1, sum2);

        // combine the sums into sum1
        sum1 <<= 32;
        sum1 += sum2;

        // write the checksum to the file
        file.write(reinterpret_cast<char*>(&sum1), sizeof(uint64_t));
      }
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
      if (comm.size() > 1)
        filename_str << comm.rank();
      std::ifstream file;
      file.open(filename_str.str(), std::ios::in | std::ios::binary);
      if (file)
      {
        // initialize the two fletcher sums
        uint64_t sum1 = 0;
        uint64_t sum2 = 0;

        // do the actual restore
        ISTLBackupRestoreHelper<V>::restore(vector, file, sum1, sum2);

        // combine the sums into sum1
        sum1 <<= 32;
        sum1 += sum2;

        // read checksum from file
        file.read(reinterpret_cast<char*>(&sum2), sizeof(uint64_t));

        // compare the checksums
        if (sum1 != sum2)
          DUNE_THROW(Dune::Exception, "Checksum mismatch when restoring binary data!");
      }
      else
        DUNE_THROW(Dune::Exception, "Could not open file to restore vector!");
      file.close();
    }
  };

}

#endif
