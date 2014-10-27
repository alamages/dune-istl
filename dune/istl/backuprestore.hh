// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifndef DUNE_ISTL_BACKUPRESTORE_HH
#define DUNE_ISTL_BACKUPRESTORE_HH

#include<fstream>

#include<dune/common/parallel/mpihelper.hh>
#include<dune/common/fvector.hh>
#include<dune/istl/bvector.hh>


namespace Dune {

  // swap endianess if on a big endian architecture
  template <typename T>
  T convertToLittleEndian(T u)
  {
#ifdef HAVE_BIGENDIAN
    union
    {
      T u;
      unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
      dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
#else
    return u;
#endif
  }

  /** \brief update a fletcher16 checksum
   * \param data the pointer to the data
   * \param length the size of the memory block to calculate the checksum of
   * \param sum1 the first half of the checksum to start with
   * \param sum2 the second half of the checksum to start with
   *
   * No checksum is returned, but instead the variables sum1 and sum2 are updated to be
   * combined to a checksum as soon as we are ready. This is done because we dont want
   * to iterate over the data twice but instead update the checksum on the fly.
   *
   * While one could argue that a 16bit checksum isnt very safe, we favor it here,
   * because of its bytewise procedure that negates all endianness issues. Its better than
   * no checksum at all, after all.
   */
  void fletcher16(const char* data, std::size_t length, uint16_t& sum1, uint16_t& sum2)
  {
    for (std::size_t i=0; i<length; ++i, ++data)
    {
      // update the checksum
      sum1 = (sum1 + *data) % 255;
      sum2 = (sum2 + sum1) % 255;
    }
  }

  template<class DATA>
  void writeToStreamWithChecksum(const DATA& data, std::ofstream& stream, uint16_t& sum1, uint16_t& sum2)
  {
    DATA buffer = convertToLittleEndian(data);
    stream.write(reinterpret_cast<const char*>(&buffer), sizeof(DATA));
    fletcher16(reinterpret_cast<const char*>(&buffer), sizeof(DATA), sum1, sum2);
  }

  template<class DATA>
  void readFromStreamWithChecksum(DATA& data, std::ifstream& stream, uint16_t& sum1, uint16_t& sum2)
  {
    DATA buffer;
    stream.read(reinterpret_cast<char*>(&buffer), sizeof(DATA));
    data = convertToLittleEndian(buffer);
    fletcher16(reinterpret_cast<const char*>(&buffer), sizeof(DATA), sum1, sum2);
  }

  template<class DATA>
  struct ISTLBackupRestoreHelper
  {
    static void backup(const DATA& data, std::ofstream& stream, uint16_t& sum1, uint16_t& sum2)
    {
      writeToStreamWithChecksum(data, stream, sum1, sum2);
    }

    static void restore(DATA& data, std::ifstream& stream, uint16_t& sum1, uint16_t& sum2)
    {
      readFromStreamWithChecksum(data, stream, sum1, sum2);
    }
  };

  template<class Block, class Allocator>
  struct ISTLBackupRestoreHelper<Dune::BlockVector<Block, Allocator> >
  {
    typedef typename Dune::BlockVector<Block, Allocator> Data;
    typedef typename Data::size_type size_type;

    static void backup(const Data& data, std::ofstream& stream, uint16_t& sum1, uint16_t& sum2)
    {
      uint64_t size = static_cast<uint64_t>(data.size());
      writeToStreamWithChecksum(size, stream, sum1, sum2);
      for (size_type i=0; i<data.size(); i++)
        ISTLBackupRestoreHelper<Block>::backup(data[i], stream, sum1, sum2);
    }

    static void restore(Data& data, std::ifstream& stream, uint16_t& sum1, uint16_t& sum2)
    {
      uint64_t size;
      readFromStreamWithChecksum(size, stream, sum1, sum2);
      data.resize(static_cast<size_type>(size));
      for (size_type i = 0; i<size; i++)
        ISTLBackupRestoreHelper<Block>::restore(data[i], stream, sum1, sum2);
    }
  };

  template<class T, int dim>
  struct ISTLBackupRestoreHelper<Dune::FieldVector<T,dim> >
  {
    typedef typename Dune::FieldVector<T,dim> Data;
    typedef typename Data::size_type size_type;

    static void backup(const Data& data, std::ofstream& stream, uint16_t& sum1, uint16_t& sum2)
    {
      for (size_type i=0; i<dim; i++)
        ISTLBackupRestoreHelper<T>::backup(data[i], stream, sum1, sum2);
    }

    static void restore(Data& data, std::ifstream& stream, uint16_t& sum1, uint16_t& sum2)
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
        uint16_t sum1 = 0;
        uint16_t sum2 = 0;

        // do the actual backup
        ISTLBackupRestoreHelper<V>::backup(vector, file, sum1, sum2);

        // combine the sums into sum1
        sum1 <<= 8;
        sum1 += sum2;

        // write the checksum to the file
        sum1 = convertToLittleEndian(sum1);
        file.write(reinterpret_cast<char*>(&sum1), sizeof(uint16_t));
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
        uint16_t sum1 = 0;
        uint16_t sum2 = 0;

        // do the actual restore
        ISTLBackupRestoreHelper<V>::restore(vector, file, sum1, sum2);

        // combine the sums into sum1
        sum1 <<= 8;
        sum1 += sum2;

        // read checksum from file
        sum1 = convertToLittleEndian(sum1);
        file.read(reinterpret_cast<char*>(&sum2), sizeof(uint16_t));

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
