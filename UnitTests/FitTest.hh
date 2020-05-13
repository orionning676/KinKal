//
// ToyMC test of fitting an KTRAJ-based KKTrk
//
#include "MatEnv/MatDBInfo.hh"
#include "MatEnv/DetMaterial.hh"
#include "KinKal/PKTraj.hh"
#include "KinKal/TLine.hh"
#include "KinKal/TPoca.hh"
#include "KinKal/StrawHit.hh"
#include "KinKal/StrawMat.hh"
#include "KinKal/ScintHit.hh"
#include "KinKal/BField.hh"
#include "KinKal/Vectors.hh"
#include "KinKal/KKConfig.hh"
#include "KinKal/KKHit.hh"
#include "KinKal/DXing.hh"
#include "KinKal/KKTrk.hh"
#include "UnitTests/ToyMC.hh"
#include "UnitTests/KKHitInfo.hh"
#include "CLHEP/Units/PhysicalConstants.h"

#include <iostream>
#include <getopt.h>
#include <typeinfo>
#include <vector>
#include <cmath>
#include <ctime>
#include <chrono>
#include <cfenv>
#include <memory>

#include "TH1F.h"
#include "TTree.h"
#include "TBranch.h"
#include "TSystem.h"
#include "THelix.h"
#include "TPolyLine3D.h"
#include "TAxis3D.h"
#include "TFile.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TVector3.h"
#include "TPolyLine3D.h"
#include "TPolyMarker3D.h"
#include "TLegend.h"
#include "TGraph.h"
#include "TRandom3.h"
#include "TH2F.h"
#include "TDirectory.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "Math/VectorUtil.h"

using namespace MatEnv;
using namespace KinKal;
using namespace std;
// avoid confusion with root
using KinKal::TLine;
void print_usage() {
  printf("Usage: FitTest  --momentum f --simparticle i --fitparticle i--charge i --nhits i --hres f --seed i --nmeta i --maxniter i --maxtemp f--ambigdoca f --ntries i --convdchisq f --simmat i--fitmat i --ttree i --Bz f --dBx f --dBy f --dBz f--Bgrad f --tollerance f--TFile c --PrintBad i --PrintDetail i --ScintHit i --UpdateHits i--addbf i --invert i\n");
}

template <class KTRAJ>
int FitTest(int argc, char **argv) {
  struct KTRAJPars{
    Float_t pars_[KTRAJ::NParams()];
    static std::string leafnames() {
      std::string names;
      for(size_t ipar=0;ipar<KTRAJ::NParams();ipar++){
	names +=KTRAJ::paramName(static_cast<typename KTRAJ::ParamIndex>(ipar)) + string("/f");
	if(ipar < KTRAJ::NParams()-1)names += ":";
      }
      return names;
    }
  };


  // define the typedefs: to change to a different trajectory implementation, just change this line
  typedef PKTraj<KTRAJ> PKTRAJ;
  typedef KKTrk<KTRAJ> KKTRK;
  typedef shared_ptr<KKConfig> KKCONFIGPTR;
  typedef THit<KTRAJ> THIT;
  typedef std::shared_ptr<THIT> THITPTR;
  typedef DXing<KTRAJ> DXING;
  typedef std::shared_ptr<DXING> DXINGPTR;
  typedef StrawHit<KTRAJ> STRAWHIT;
  typedef std::shared_ptr<STRAWHIT> STRAWHITPTR;
  typedef ScintHit<KTRAJ> SCINTHIT;
  typedef std::shared_ptr<SCINTHIT> SCINTHITPTR;
  typedef StrawXing<KTRAJ> STRAWXING;
  typedef shared_ptr<STRAWXING> STRAWXINGPTR;
  typedef vector<THITPTR> THITCOL;
  typedef vector<DXINGPTR> DXINGCOL;
  typedef Residual<KTRAJ::NParams()> RESIDUAL;
  typedef TPoca<PKTRAJ,TLine> TPOCA;
  typedef std::chrono::high_resolution_clock Clock;

  // enable throw on FPE; not working with clang!
  fetestexcept(FE_ALL_EXCEPT );
  // parameters
  int opt;
  double mom(105.0);
  double masses[5]={0.511,105.66,139.57, 493.68, 938.0};
  int isimmass(0), ifitmass(0), icharge(-1);
  double simmass, fitmass;
  unsigned maxniter(5);
  unsigned nmeta(2);
  double maxtemp(0.0);
  unsigned ntries(1000);
  double convdchisq(0.1);
  bool ttree(true), printbad(false);
  string tfname("FitTest.root");
  int detail(0), invert(0);
  double ambigdoca(-1.0);// minimum doca to set ambiguity, default sets for all hits
  bool updatehits(false), addbf(false), fitmat(true);
  vector<double> sigmas = { 3.0, 3.0, 3.0, 3.0, 0.1, 3.0}; // base sigmas for parameter plots
  BField *BF(0);
  double Bgrad(0.0), dBx(0.0), dBy(0.0), dBz(0.0), Bz(1.0);
  double zrange(3000);
  double tol(0.1);
  int iseed(123421);
  unsigned nhits(40);
  bool simmat(true), lighthit(true);

  static struct option long_options[] = {
    {"momentum",     required_argument, 0, 'm' },
    {"simparticle",     required_argument, 0, 'S'  },
    {"fitparticle",     required_argument, 0, 'F'  },
    {"charge",     required_argument, 0, 'q'  },
    {"seed",     required_argument, 0, 's'  },
    {"hres",     required_argument, 0, 'h'  },
    {"nhits",     required_argument, 0, 'n'  },
    {"escale",     required_argument, 0, 'e'  },
    {"maxniter",     required_argument, 0, 'i'  },
    {"nmeta",     required_argument, 0, 'M'  },
    {"maxtemp",     required_argument, 0, 'X'  },
    {"simmat",     required_argument, 0, 'b'  },
    {"fitmat",     required_argument, 0, 'f'  },
    {"ambigdoca",     required_argument, 0, 'd'  },
    {"ntries",     required_argument, 0, 'N'  },
    {"convdchisq",     required_argument, 0, 'C'  },
    {"ttree",     required_argument, 0, 'r'  },
    {"tolerance",     required_argument, 0, 't'  },
    {"TFile",     required_argument, 0, 'T'  },
    {"dBx",     required_argument, 0, 'x'  },
    {"dBy",     required_argument, 0, 'y'  },
    {"dBz",     required_argument, 0, 'Z'  },
    {"Bz",     required_argument, 0, 'z'  },
    {"Bgrad",     required_argument, 0, 'g'  },
    {"PrintBad",     required_argument, 0, 'P'  },
    {"PrintDetail",     required_argument, 0, 'D'  },
    {"ScintHit",     required_argument, 0, 'L'  },
    {"UpdateHits",     required_argument, 0, 'U'  },
    {"addbf",     required_argument, 0, 'B'  },
    {"invert",     required_argument, 0, 'I'  },
    {NULL, 0,0,0}
  };

  int long_index =0;
  while ((opt = getopt_long_only(argc, argv,"",
	  long_options, &long_index )) != -1) {
    switch (opt) {
      case 'm' : mom = atof(optarg);
		 break;
      case 'S' : isimmass = atoi(optarg);
		 break;
      case 'F' : ifitmass = atoi(optarg);
		 break;
      case 'q' : icharge = atoi(optarg);
		 break;
      case 'z' : Bz = atof(optarg);
		 break;
      case 'n' : nhits = atoi(optarg);
		 break;
      case 's' : iseed = atoi(optarg);
		 break;
      case 'M' : nmeta = atoi(optarg);
		 break;
      case 'i' : maxniter = atoi(optarg);
		 break;
      case 'X' : maxtemp = atof(optarg);
		 break;
      case 'b' : simmat = atoi(optarg);
		 break;
      case 'f' : fitmat = atoi(optarg);
		 break;
      case 'L' : lighthit = atoi(optarg);
		 break;
      case 'U' : updatehits = atoi(optarg);
		 break;
      case 'B' : addbf = atoi(optarg);
		 break;
      case 'r' : ttree = atoi(optarg);
		 break;
      case 'd' : ambigdoca = atof(optarg);
		 break;
      case 'N' : ntries = atoi(optarg);
		 break;
      case 'C' : convdchisq= atof(optarg);
		 break;
      case 'x' : dBx = atof(optarg);
		 break;
      case 'y' : dBy = atof(optarg);
		 break;
      case 'Z' : dBz = atof(optarg);
		 break;
      case 'g' : Bgrad = atof(optarg);
		 break;
      case 'P' : printbad = atoi(optarg);
		 break;
      case 'D' : detail = atoi(optarg);
		 break;
      case 'I' : invert = atoi(optarg);
		 break;
      case 't' : tol = atof(optarg);
		 break;
      case 'T' : tfname = optarg;
		 break;
      default: print_usage();
	       exit(EXIT_FAILURE);
    }
  }

  // construct BField
  Vec3 bnom;
  if(Bgrad != 0){
    BF = new GradBField(Bz-0.5*Bgrad,Bz+0.5*Bgrad,-0.5*zrange,0.5*zrange);
    bnom = BF->fieldVect(Vec3(0.0,0.0,0.0));
  } else {
    Vec3 bsim(dBx,dBy,Bz+dBz);
    BF = new UniformBField(bsim);
    bnom = Vec3(0.0,0.0,Bz);
  }
  // create ToyMC
  simmass = masses[isimmass];
  fitmass = masses[ifitmass];
  KKTest::ToyMC<KTRAJ> toy(*BF, mom, icharge, zrange, iseed, nhits, simmat, lighthit, ambigdoca, simmass );
  // generate hits
  THITCOL thits; // this program shares hit ownership with KKTrk
  DXINGCOL dxings; // this program shares det xing ownership with KKTrk
  PKTRAJ tptraj;
  toy.simulateParticle(tptraj, thits, dxings);
  // temporary FIXME!
  toy.setSmearSeed(false);
  cout << "True initial " << tptraj.front() << endl;
//  cout << "vector of hit points " << thits.size() << endl;
//  cout << "True " << tptraj << endl;
  double startmom = tptraj.momentumMag(tptraj.range().low());
  double endmom = tptraj.momentumMag(tptraj.range().high());
  Vec3 end, bend;
  bend = tptraj.front().direction(tptraj.range().high());
  end = tptraj.back().direction(tptraj.range().high());
  double angle = ROOT::Math::VectorUtil::Angle(bend,end);
  cout << "total momentum change = " << endmom-startmom << " total angle change = " << angle << endl;
  // create the fit seed by randomizing the parameters at the middle.  Overrwrite to use the fit BField
  auto const& midhel = tptraj.nearestPiece(0.0);
  KTRAJ seedtraj(midhel.params(),fitmass,midhel.charge(),bnom,midhel.range());
  if(invert) seedtraj.invertCT(); // for testing wrong propagation direction
  toy.createSeed(seedtraj);
  cout << "Seed params " << seedtraj.params().parameters() <<" covariance " << endl << seedtraj.params().covariance() << endl;
  // Create the KKTrk from these hits
  //
  KKCONFIGPTR configptr = make_shared<KKConfig>(*BF);
  configptr->dwt_ = 1.0e6;
  configptr->maxniter_ = maxniter;
  configptr->addbf_ = addbf;
  configptr->addmat_ = fitmat;
  configptr->tol_ = tol;
  configptr->plevel_ = (KKConfig::printLevel)detail;
  // add schedule; MC-truth based ambiguity
  MConfig mconfig;
  mconfig.updatemat_ = mconfig.updatebfcorr_ = false ;
  mconfig.updatehits_ = updatehits;
  mconfig.hitupdateparams_.push_back(make_any<WHUParams>(ambigdoca,100.0)); // 1st parameter turns off drift, 2nd says accept all hits
  mconfig.temp_ = maxtemp; // first
//  mconfig.convdchisq_ = 1.0; // initially crude 
  mconfig.convdchisq_ = convdchisq; 
  mconfig.divdchisq_ = 1000*mconfig.convdchisq_;
  mconfig.oscdchisq_ = 10*mconfig.convdchisq_;
  configptr->schedule_.push_back(mconfig);
  double tstep = maxtemp/(std::max(nmeta,(unsigned)1));
  double temp = maxtemp;
  for(unsigned imeta = 0; imeta< nmeta; imeta++){
    temp -= tstep;
    mconfig.temp_ = temp;
    mconfig.updatemat_ = fitmat;
    mconfig.updatebfcorr_ = true;
    configptr->schedule_.push_back(mconfig);
  }
  cout << *configptr << endl;
// create and fit the track
  KKTRK kktrk(configptr,seedtraj,thits,dxings);
  kktrk.print(cout,detail);
  TFile fitfile(tfname.c_str(),"RECREATE");
  // tree variables
  KTRAJPars ftpars_, etpars_, spars_, ffitpars_, ffiterrs_, efitpars_, efiterrs_;
  float chisq_, etmom_, ftmom_, ffmom_, efmom_, chiprob_;
  float fft_,eft_;
  int ndof_, niter_, status_, igap_;
  float maxgap_, avgap_;

  if(ntries <=0 ){
    // draw the fit result
    TCanvas* pttcan = new TCanvas("pttcan","PieceKTRAJ",1000,1000);
    auto const& fithel = kktrk.fitTraj();
    unsigned np = fithel.range().range()*fithel.speed(fithel.range().mid());
    TPolyLine3D* fitpl = new TPolyLine3D(np);
    fitpl->SetLineColor(kBlue);
    fitpl->SetLineStyle(kSolid);
    double ts = fithel.range().range()/(np-1);
    for(unsigned ip=0;ip<np;ip++){
      double tp = fithel.range().low() + ip*ts;
      Vec3 ppos = fithel.position(tp);
      fitpl->SetPoint(ip,ppos.X(),ppos.Y(),ppos.Z());
    }
    fitpl->Draw();
// now draw the truth
    TPolyLine3D* ttpl = new TPolyLine3D(np);
    ttpl->SetLineColor(kGreen);
    ttpl->SetLineStyle(kDashDotted);
    ts = tptraj.range().range()/(np-1);
    for(unsigned ip=0;ip<np;ip++){
      double tp = tptraj.range().low() + ip*ts;
      Vec3 ppos = tptraj.position(tp);
      ttpl->SetPoint(ip,ppos.X(),ppos.Y(),ppos.Z());
    }
    ttpl->Draw();
    // draw the hits
    std::vector<TPolyLine3D*> htpls;
    for(auto const& thit : thits) {
      TPolyLine3D* line = new TPolyLine3D(2);
      Vec3 plow, phigh;
      STRAWHITPTR shptr = std::dynamic_pointer_cast<STRAWHIT> (thit); 
      SCINTHITPTR lhptr = std::dynamic_pointer_cast<SCINTHIT> (thit);
      if(shptr.use_count() > 0){
	auto const& tline = shptr->wire();
	plow = tline.position(tline.range().low());
	phigh = tline.position(tline.range().high());
	line->SetLineColor(kRed);
      } else if (lhptr.use_count() > 0){
	auto const& tline = lhptr->sensorAxis();
	plow = tline.position(tline.range().low());
	phigh = tline.position(tline.range().high());
	line->SetLineColor(kCyan);
      }
      line->SetPoint(0,plow.X(),plow.Y(), plow.Z());
      line->SetPoint(1,phigh.X(),phigh.Y(), phigh.Z());
      line->Draw();
      htpls.push_back(line);
    }

    // draw the origin and axes
    TAxis3D* rulers = new TAxis3D();
    rulers->GetXaxis()->SetAxisColor(kBlue);
    rulers->GetXaxis()->SetLabelColor(kBlue);
    rulers->GetYaxis()->SetAxisColor(kCyan);
    rulers->GetYaxis()->SetLabelColor(kCyan);
    rulers->GetZaxis()->SetAxisColor(kOrange);
    rulers->GetZaxis()->SetLabelColor(kOrange);
    rulers->Draw();
    pttcan->Write();

  } else {
    TTree* ftree(0);
    KKHIV hinfo;
    if(ttree){
      ftree = new TTree("fit","fit");
      ftree->Branch("ftpars.", &ftpars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("etpars.", &etpars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("spars.", &spars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("ffpars.", &ffitpars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("fferrs.", &ffiterrs_,KTRAJPars::leafnames().c_str());
      ftree->Branch("efpars.", &efitpars_,KTRAJPars::leafnames().c_str());
      ftree->Branch("eferrs.", &efiterrs_,KTRAJPars::leafnames().c_str());
      ftree->Branch("chisq", &chisq_,"chisq/F");
      ftree->Branch("ndof", &ndof_,"ndof/I");
      ftree->Branch("chiprob", &chiprob_,"chiprob/F");
      ftree->Branch("niter", &niter_,"niter/I");
      ftree->Branch("status", &status_,"status/I");
      ftree->Branch("ftmom", &ftmom_,"ftmom/F");
      ftree->Branch("etmom", &etmom_,"etmom/F");
      ftree->Branch("ffmom", &ffmom_,"ffmom/F");
      ftree->Branch("efmom", &efmom_,"efmom/F");
      ftree->Branch("fft", &fft_,"fft/F");
      ftree->Branch("eft", &eft_,"eft/F");
      ftree->Branch("maxgap", &maxgap_,"maxgap/F");
      ftree->Branch("avgap", &avgap_,"avgap/F");
      ftree->Branch("igap", &igap_,"igap/I");
      ftree->Branch("hinfo",&hinfo);
    }
    // now repeat this to gain statistics
    vector<TH1F*> fdp(KTRAJ::NParams());
    vector<TH1F*> bdp(KTRAJ::NParams());
    vector<TH1F*> fpull(KTRAJ::NParams());
    vector<TH1F*> bpull(KTRAJ::NParams());
    vector<TH1F*> fiterrh(KTRAJ::NParams());
    TH1F* hniter = new TH1F("niter", "Total Iterations", 50,-0.5,49.5);
    TH1F* hnfail = new TH1F("nfail", "Failed Iterations", 50,-0.5,49.5);
    TH1F* hndiv = new TH1F("ndiv", "Diverged Iterations", 50,-0.5,49.5);
    hnfail->SetLineColor(kRed);
    hndiv->SetLineColor(kOrange);
    TH1F* ndof = new TH1F("ndof", "N Degree of Freedom", 100,-0.5,99.5);
    TH1F* chisq = new TH1F("chisq", "Chisquared", 100,0,100);
    TH1F* chisqndof = new TH1F("chisqndof", "Chisquared per NDOF", 100,0,10.0);
    TH1F* chisqprob = new TH1F("chisqprob", "Chisquared probability", 100,0,1.0);
    TH1F* logchisqprob = new TH1F("logchisqprob", "Chisquared probability", 100,-10,0.0);
    string htitle, hname;
    TH2F* corravg = new TH2F("corravg","Average correlation matrix magnitudes",KTRAJ::NParams(),-0.5,KTRAJ::NParams()-0.5,KTRAJ::NParams(), -0.5,KTRAJ::NParams()-0.5);
    TAxis* xax = corravg->GetXaxis();
    TAxis* yax = corravg->GetYaxis();
    double nsig(10.0);
    double pscale = nsig/sqrt(nhits);
    for(size_t ipar=0;ipar< KTRAJ::NParams(); ipar++){
      auto tpar = static_cast<typename KTRAJ::ParamIndex>(ipar);
      hname = string("fd") + KTRAJ::paramName(tpar);
      htitle = string("Front #Delta ") + KTRAJ::paramTitle(tpar);
      fdp[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,-pscale*sigmas[ipar],pscale*sigmas[ipar]);
      hname = string("bd") + KTRAJ::paramName(tpar);
      htitle = string("Back #Delta ") + KTRAJ::paramTitle(tpar);
      bdp[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,-pscale*sigmas[ipar],pscale*sigmas[ipar]);
      hname = string("fp") + KTRAJ::paramName(tpar);
      htitle = string("Front Pull ") + KTRAJ::paramTitle(tpar);
      fpull[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,-nsig,nsig);
      hname = string("bp") + KTRAJ::paramName(tpar);
      htitle = string("Back Pull ") + KTRAJ::paramTitle(tpar);
      bpull[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,-nsig,nsig);
      hname = string("e") + KTRAJ::paramName(tpar);
      htitle = string("Error ") + KTRAJ::paramTitle(tpar);
      fiterrh[ipar] = new TH1F(hname.c_str(),htitle.c_str(),100,0.0,pscale*sigmas[ipar]);
      xax->SetBinLabel(ipar+1,KTRAJ::paramName(tpar).c_str());
      yax->SetBinLabel(ipar+1,KTRAJ::paramName(tpar).c_str());
    }
    double duration (0.0);
    for(unsigned itry=0;itry<ntries;itry++){
    // create a random true initial helix with hits and material interactions from this.  This also handles BField inhomogeneity truth tracking
      PKTRAJ tptraj;
      thits.clear();
      dxings.clear();
      toy.simulateParticle(tptraj,thits,dxings);
      auto const& midhel = tptraj.nearestPiece(tptraj.range().mid());
      KTRAJ seedtraj(midhel.params(),fitmass,midhel.charge(),bnom,midhel.range());
      if(invert)seedtraj.invertCT();
      toy.createSeed(seedtraj);
      auto start = Clock::now();
      KKTRK kktrk(configptr,seedtraj,thits,dxings);
      auto stop = Clock::now();
      duration += std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
      // compare parameters at the first traj of both true and fit
      // correct the true parameters in case the BField isn't nominal
      KTRAJ const& fftraj = kktrk.fitTraj().nearestPiece(tptraj.range().low());
      KTRAJ const& bftraj = kktrk.fitTraj().nearestPiece(tptraj.range().high());
      KTRAJ const& fttraj = tptraj.nearestPiece(tptraj.range().low());
      KTRAJ const& bttraj = tptraj.nearestPiece(tptraj.range().high());
      typename KTRAJ::PDATA ftpars, btpars; 
      if((fftraj.bnom() - fttraj.bnom()).R() < 1e-6){
	ftpars = fftraj.params();
	btpars = bftraj.params();
      } else {
	Mom4 fmom, bmom;
	Vec4 fpos, bpos;
	fftraj.position(fpos);
	bftraj.position(bpos);
	fpos.SetE(fttraj.range().mid());
	bpos.SetE(bttraj.range().mid());
	fmom = fttraj.momentum(fpos.T());
	bmom = bttraj.momentum(bpos.T());
	KTRAJ ft(fpos,fmom,tptraj.charge(),fftraj.bnom());
	KTRAJ bt(bpos,bmom,tptraj.charge(),bftraj.bnom());
	ftpars = ft.params();
	btpars = bt.params();
      }
      // fit parameters
      auto const& ffpars = fftraj.params();
      auto const& bfpars = bftraj.params();
      double maxgap, avgap;
      size_t igap;
      kktrk.fitTraj().gaps(maxgap, igap, avgap);
      maxgap_ = maxgap;
      avgap_ = avgap;
      igap_ = igap;

      // momentum
      // accumulate parameter difference and pull
      vector<double> cerr(6,0.0), bcerr(6,0.0);
      for(size_t ipar=0;ipar< KTRAJ::NParams(); ipar++){
	cerr[ipar] = sqrt(ffpars.covariance()[ipar][ipar]);
	bcerr[ipar] = sqrt(bfpars.covariance()[ipar][ipar]);
	fdp[ipar]->Fill(ffpars.parameters()[ipar]-ftpars.parameters()[ipar]);
	bdp[ipar]->Fill(bfpars.parameters()[ipar]-btpars.parameters()[ipar]);
	fpull[ipar]->Fill((ffpars.parameters()[ipar]-ftpars.parameters()[ipar])/cerr[ipar]);
	bpull[ipar]->Fill((bfpars.parameters()[ipar]-btpars.parameters()[ipar])/bcerr[ipar]);
	fiterrh[ipar]->Fill(cerr[ipar]);
      }
      // accumulate average correlation matrix
      auto const& cov = ffpars.covariance();
      //    auto cormat = cov;
      for(unsigned ipar=0; ipar <KTRAJ::NParams();ipar++){
	for(unsigned jpar=ipar;jpar < KTRAJ::NParams(); jpar++){
	  double corr = cov[ipar][jpar]/(cerr[ipar]*cerr[jpar]);
	  //	cormat[ipar][jpar] = corr;
	  corravg->Fill(ipar,jpar,fabs(corr));
	}
      }
      // accumulate chisquared info
      unsigned niter(0), nfail(0), ndiv(0);
      for(auto const& fstat: kktrk.history()){
	if(fstat.status_ != FitStatus::needsfit)niter++;
	if(fstat.status_ == FitStatus::failed)nfail++;
	if(fstat.status_ == FitStatus::diverged)ndiv++;
      }
      hniter->Fill(niter);
      hnfail->Fill(nfail);
      hndiv->Fill(ndiv);
      auto const& fstat = kktrk.fitStatus();
      chiprob_ = fstat.prob_; 
      ndof->Fill(fstat.ndof_);
      chisq->Fill(fstat.chisq_);
      chisqndof->Fill(fstat.chisq_/fstat.ndof_);
      chisqprob->Fill(chiprob_);
      logchisqprob->Fill(log10(chiprob_));
      // fill tree
      for(size_t ipar=0;ipar<6;ipar++){
	spars_.pars_[ipar] = seedtraj.params().parameters()[ipar];
	ftpars_.pars_[ipar] = fttraj.params().parameters()[ipar];
	etpars_.pars_[ipar] = bttraj.params().parameters()[ipar];
	ffitpars_.pars_[ipar] = fftraj.params().parameters()[ipar];
	efitpars_.pars_[ipar] = bftraj.params().parameters()[ipar];
	ffiterrs_.pars_[ipar] = sqrt(fftraj.params().covariance()(ipar,ipar));
	efiterrs_.pars_[ipar] = sqrt(bftraj.params().covariance()(ipar,ipar));
      }
      ftmom_ = fttraj.momentumMag(tptraj.range().low());
      etmom_ = bttraj.momentumMag(tptraj.range().high());
      ffmom_ = fftraj.momentumMag(tptraj.range().low());
      efmom_ = bftraj.momentumMag(tptraj.range().high());
      fft_ = kktrk.fitTraj().range().low();
      eft_ = kktrk.fitTraj().range().high();
      chisq_ = fstat.chisq_;
      ndof_ = fstat.ndof_;
      niter_ = fstat.iter_;
      status_ = fstat.status_;

      // test
      if(printbad && !kktrk.fitStatus().usable()){
	cout << "Bad Fit try " << itry << endl;
	cout << "True Traj " << tptraj << endl;
	cout << "Seed Traj " << seedtraj << endl;
	kktrk.print(cout,detail);
      }
      if(ttree)ftree->Fill();
    }
    cout <<"Time/fit = " << duration/double(ntries) << " Nanoseconds " << endl;
    // fill canvases
    TCanvas* fdpcan = new TCanvas("fdpcan","fdpcan",800,600);
    fdpcan->Divide(3,2);
    for(size_t ipar=0;ipar<KTRAJ::NParams();++ipar){
      fdpcan->cd(ipar+1);
      fdp[ipar]->Fit("gaus","q");
    }
    fdpcan->Write();
    TCanvas* bdpcan = new TCanvas("bdpcan","bdpcan",800,600);
    bdpcan->Divide(3,2);
    for(size_t ipar=0;ipar<KTRAJ::NParams();++ipar){
      bdpcan->cd(ipar+1);
      bdp[ipar]->Fit("gaus","q");
    }
    bdpcan->Write();
    TCanvas* fpullcan = new TCanvas("fpullcan","fpullcan",800,600);
    fpullcan->Divide(3,2);
    for(size_t ipar=0;ipar<KTRAJ::NParams();++ipar){
      fpullcan->cd(ipar+1);
      fpull[ipar]->Fit("gaus","q");
    }
    fpullcan->Write();
    TCanvas* bpullcan = new TCanvas("bpullcan","bpullcan",800,600);
    bpullcan->Divide(3,2);
    for(size_t ipar=0;ipar<KTRAJ::NParams();++ipar){
      bpullcan->cd(ipar+1);
      bpull[ipar]->Fit("gaus","q");
    }
    bpullcan->Write();
    TCanvas* perrcan = new TCanvas("perrcan","perrcan",800,600);
    perrcan->Divide(3,2);
    for(size_t ipar=0;ipar<KTRAJ::NParams();++ipar){
      perrcan->cd(ipar+1);
      fiterrh[ipar]->Draw();
    }
    perrcan->Write();
    TCanvas* corrcan = new TCanvas("corrcan","corrcan",600,600);
    corrcan->Divide(1,1);
    corrcan->cd(1);
    corravg->Scale(1.0/double(ntries));
    corravg->SetStats(0);
    gPad->SetLogz();
    corravg->Draw("colorztext0");
    corrcan->Write();

    TCanvas* statuscan = new TCanvas("statuscan","statuscan",800,600);
    statuscan->Divide(3,2);
    statuscan->cd(1);
    hniter->Draw();
    hnfail->Draw("same");
    hndiv->Draw("same");
    statuscan->cd(2);
    ndof->Draw();
    statuscan->cd(3);
    chisq->Draw();
    statuscan->cd(4);
    chisqndof->Draw();
    statuscan->cd(5);
    chisqprob->Draw();
    statuscan->cd(6);
    logchisqprob->Draw();
    statuscan->Write();
  }
  fitfile.Write();
  fitfile.Close();
  exit(EXIT_SUCCESS);
}