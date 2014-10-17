// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#include<dune/common/fvector.hh>
#include<dune/istl/bvector.hh>
#include<dune/istl/backuprestore.hh>


int main()
{

  typedef Dune::BlockVector<Dune::FieldVector<double, 2> > V;

  V v;
  v.resize(100);
  std::fill(v.begin(), v.end(), 5);

  Dune::ISTLBackupRestoreFacility<V>::backup(v,"testbackup", Dune::FakeMPIHelper::instance(0,NULL));

  V w;
  Dune::ISTLBackupRestoreFacility<V>::restore(w,"testbackup", Dune::FakeMPIHelper::instance(0,NULL));
  Dune::ISTLBackupRestoreFacility<V>::backup(w,"testbackup2", Dune::FakeMPIHelper::instance(0,NULL));

  // test nestedness
  typedef Dune::BlockVector<Dune::BlockVector<Dune::FieldVector<double,1> > > NV;
  NV nv;
  nv.resize(10);
  for (int i=0; i<10; i++)
  {
    nv[i].resize(i);
    std::fill(nv[i].begin(), nv[i].end(), i);
  }

  Dune::ISTLBackupRestoreFacility<NV>::backup(nv,"testbackup_nestes", Dune::FakeMPIHelper::instance(0,NULL));

  return 0;
}
