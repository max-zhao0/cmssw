#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Run.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Utilities/interface/EDGetToken.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "DataFormats/Common/interface/Handle.h"

#include "SimTracker/TrackTriggerAssociation/interface/StubAssociation.h"
#include "L1Trigger/TrackTrigger/interface/Setup.h"
#include "L1Trigger/TrackerTFP/interface/DataFormats.h"
#include "L1Trigger/TrackFindingTracklet/interface/ChannelAssignment.h"

#include <TProfile.h>
#include <TH1F.h>

#include <vector>
#include <deque>
#include <set>
#include <cmath>
#include <numeric>
#include <sstream>

using namespace std;
using namespace edm;
using namespace trackerTFP;
using namespace tt;

namespace trklet {

  /*! \class  trklet::AnalyzerKFin
   *  \brief  Class to analyze hardware like structured TTStub Collection generated by Tracklet
   *  \author Thomas Schuh
   *  \date   2020, Nov
   */
  class AnalyzerKFin : public one::EDAnalyzer<one::WatchRuns, one::SharedResources> {
  public:
    AnalyzerKFin(const ParameterSet& iConfig);
    void beginJob() override {}
    void beginRun(const Run& iEvent, const EventSetup& iSetup) override;
    void analyze(const Event& iEvent, const EventSetup& iSetup) override;
    void endRun(const Run& iEvent, const EventSetup& iSetup) override {}
    void endJob() override;

  private:
    //
    void formTracks(const StreamsTrack& streamsTrack,
                    const StreamsStub& streamsStubs,
                    vector<vector<TTStubRef>>& tracks,
                    int channel) const;
    //
    void associate(const vector<vector<TTStubRef>>& tracks,
                   const StubAssociation* ass,
                   set<TPPtr>& tps,
                   int& sum,
                   bool perfect = false) const;

    // ED input token of stubs
    EDGetTokenT<StreamsStub> edGetTokenAcceptedStubs_;
    // ED input token of tracks
    EDGetTokenT<StreamsTrack> edGetTokenAcceptedTracks_;
    // ED input token of lost stubs
    EDGetTokenT<StreamsStub> edGetTokenLostStubs_;
    // ED input token of lost tracks
    EDGetTokenT<StreamsTrack> edGetTokenLostTracks_;
    // ED input token of TTStubRef to TPPtr association for tracking efficiency
    EDGetTokenT<StubAssociation> edGetTokenSelection_;
    // ED input token of TTStubRef to recontructable TPPtr association
    EDGetTokenT<StubAssociation> edGetTokenReconstructable_;
    // Setup token
    ESGetToken<Setup, SetupRcd> esGetTokenSetup_;
    // DataFormats token
    ESGetToken<DataFormats, DataFormatsRcd> esGetTokenDataFormats_;
    // ChannelAssignment token
    ESGetToken<ChannelAssignment, ChannelAssignmentRcd> esGetTokenChannelAssignment_;
    // stores, calculates and provides run-time constants
    const Setup* setup_ = nullptr;
    // helper class to extract structured data from tt::Frames
    const DataFormats* dataFormats_ = nullptr;
    // helper class to assign tracklet track to channel
    const ChannelAssignment* channelAssignment_ = nullptr;
    // enables analyze of TPs
    bool useMCTruth_;
    //
    int nEvents_ = 0;

    // Histograms

    TProfile* prof_;
    TProfile* profChannel_;
    TH1F* hisChannel_;

    // printout
    stringstream log_;
  };

  AnalyzerKFin::AnalyzerKFin(const ParameterSet& iConfig) : useMCTruth_(iConfig.getParameter<bool>("UseMCTruth")) {
    usesResource("TFileService");
    // book in- and output ED products
    const string& label = iConfig.getParameter<string>("LabelKFin");
    const string& branchAcceptedStubs = iConfig.getParameter<string>("BranchAcceptedStubs");
    const string& branchAcceptedTracks = iConfig.getParameter<string>("BranchAcceptedTracks");
    const string& branchLostStubs = iConfig.getParameter<string>("BranchLostStubs");
    const string& branchLostTracks = iConfig.getParameter<string>("BranchLostTracks");
    edGetTokenAcceptedStubs_ = consumes<StreamsStub>(InputTag(label, branchAcceptedStubs));
    edGetTokenAcceptedTracks_ = consumes<StreamsTrack>(InputTag(label, branchAcceptedTracks));
    edGetTokenLostStubs_ = consumes<StreamsStub>(InputTag(label, branchLostStubs));
    edGetTokenLostTracks_ = consumes<StreamsTrack>(InputTag(label, branchLostTracks));
    if (useMCTruth_) {
      const auto& inputTagSelecttion = iConfig.getParameter<InputTag>("InputTagSelection");
      const auto& inputTagReconstructable = iConfig.getParameter<InputTag>("InputTagReconstructable");
      edGetTokenSelection_ = consumes<StubAssociation>(inputTagSelecttion);
      edGetTokenReconstructable_ = consumes<StubAssociation>(inputTagReconstructable);
    }
    // book ES products
    esGetTokenSetup_ = esConsumes<Setup, SetupRcd, Transition::BeginRun>();
    esGetTokenDataFormats_ = esConsumes<DataFormats, DataFormatsRcd, Transition::BeginRun>();
    esGetTokenChannelAssignment_ = esConsumes<ChannelAssignment, ChannelAssignmentRcd, Transition::BeginRun>();
    // log config
    log_.setf(ios::fixed, ios::floatfield);
    log_.precision(4);
  }

  void AnalyzerKFin::beginRun(const Run& iEvent, const EventSetup& iSetup) {
    // helper class to store configurations
    setup_ = &iSetup.getData(esGetTokenSetup_);
    // helper class to extract structured data from tt::Frames
    dataFormats_ = &iSetup.getData(esGetTokenDataFormats_);
    // helper class to assign tracklet track to channel
    channelAssignment_ = &iSetup.getData(esGetTokenChannelAssignment_);
    // book histograms
    Service<TFileService> fs;
    TFileDirectory dir;
    dir = fs->mkdir("KFin");
    prof_ = dir.make<TProfile>("Counts", ";", 10, 0.5, 10.5);
    prof_->GetXaxis()->SetBinLabel(1, "Stubs");
    prof_->GetXaxis()->SetBinLabel(2, "Tracks");
    prof_->GetXaxis()->SetBinLabel(3, "Lost Tracks");
    prof_->GetXaxis()->SetBinLabel(4, "Matched Tracks");
    prof_->GetXaxis()->SetBinLabel(5, "All Tracks");
    prof_->GetXaxis()->SetBinLabel(6, "Found TPs");
    prof_->GetXaxis()->SetBinLabel(7, "Found selected TPs");
    prof_->GetXaxis()->SetBinLabel(8, "Lost TPs");
    prof_->GetXaxis()->SetBinLabel(9, "All TPs");
    prof_->GetXaxis()->SetBinLabel(10, "Perfect TPs");
    // channel occupancy
    constexpr int maxOcc = 180;
    const int numChannels = channelAssignment_->numChannelsTrack() * setup_->numLayers() * setup_->numRegions();
    hisChannel_ = dir.make<TH1F>("His Channel Occupancy", ";", maxOcc, -.5, maxOcc - .5);
    profChannel_ = dir.make<TProfile>("Prof Channel Occupancy", ";", numChannels, -.5, numChannels - .5);
  }

  void AnalyzerKFin::analyze(const Event& iEvent, const EventSetup& iSetup) {
    // read in ht products
    Handle<StreamsStub> handleAcceptedStubs;
    iEvent.getByToken<StreamsStub>(edGetTokenAcceptedStubs_, handleAcceptedStubs);
    const StreamsStub& acceptedStubs = *handleAcceptedStubs;
    Handle<StreamsTrack> handleAcceptedTracks;
    iEvent.getByToken<StreamsTrack>(edGetTokenAcceptedTracks_, handleAcceptedTracks);
    const StreamsTrack& acceptedTracks = *handleAcceptedTracks;
    Handle<StreamsStub> handleLostStubs;
    iEvent.getByToken<StreamsStub>(edGetTokenLostStubs_, handleLostStubs);
    const StreamsStub& lostStubs = *handleLostStubs;
    Handle<StreamsTrack> handleLostTracks;
    iEvent.getByToken<StreamsTrack>(edGetTokenLostTracks_, handleLostTracks);
    const StreamsTrack& lostTracks = *handleLostTracks;
    // read in MCTruth
    const StubAssociation* selection = nullptr;
    const StubAssociation* reconstructable = nullptr;
    if (useMCTruth_) {
      Handle<StubAssociation> handleSelection;
      iEvent.getByToken<StubAssociation>(edGetTokenSelection_, handleSelection);
      selection = handleSelection.product();
      prof_->Fill(9, selection->numTPs());
      Handle<StubAssociation> handleReconstructable;
      iEvent.getByToken<StubAssociation>(edGetTokenReconstructable_, handleReconstructable);
      reconstructable = handleReconstructable.product();
    }
    // analyze ht products and associate found tracks with reconstrucable TrackingParticles
    set<TPPtr> tpPtrs;
    set<TPPtr> tpPtrsSelection;
    set<TPPtr> tpPtrsPerfect;
    set<TPPtr> tpPtrsLost;
    int allMatched(0);
    int allTracks(0);
    for (int region = 0; region < setup_->numRegions(); region++) {
      const int offset = region * channelAssignment_->numChannelsTrack();
      int nStubs(0);
      int nTracks(0);
      int nLost(0);
      for (int channel = 0; channel < channelAssignment_->numChannelsTrack(); channel++) {
        vector<vector<TTStubRef>> tracks;
        formTracks(acceptedTracks, acceptedStubs, tracks, offset + channel);
        vector<vector<TTStubRef>> lost;
        formTracks(lostTracks, lostStubs, lost, offset + channel);
        nTracks += tracks.size();
        nStubs += accumulate(tracks.begin(), tracks.end(), 0, [](int sum, const vector<TTStubRef>& track) {
          return sum + (int)track.size();
        });
        nLost += lost.size();
        allTracks += tracks.size();
        if (!useMCTruth_)
          continue;
        int tmp(0);
        associate(tracks, selection, tpPtrsSelection, tmp);
        associate(tracks, selection, tpPtrsPerfect, tmp, true);
        associate(lost, selection, tpPtrsLost, tmp);
        associate(tracks, reconstructable, tpPtrs, allMatched);
      }
      prof_->Fill(1, nStubs);
      prof_->Fill(2, nTracks);
      prof_->Fill(3, nLost);
    }
    vector<TPPtr> recovered;
    recovered.reserve(tpPtrsLost.size());
    set_intersection(tpPtrsLost.begin(), tpPtrsLost.end(), tpPtrs.begin(), tpPtrs.end(), back_inserter(recovered));
    for (const TPPtr& tpPtr : recovered)
      tpPtrsLost.erase(tpPtr);
    prof_->Fill(4, allMatched);
    prof_->Fill(5, allTracks);
    prof_->Fill(6, tpPtrs.size());
    prof_->Fill(7, tpPtrsSelection.size());
    prof_->Fill(8, tpPtrsLost.size());
    prof_->Fill(10, tpPtrsPerfect.size());
    nEvents_++;
  }

  void AnalyzerKFin::endJob() {
    if (nEvents_ == 0)
      return;
    // printout SF summary
    const double totalTPs = prof_->GetBinContent(9);
    const double numStubs = prof_->GetBinContent(1);
    const double numTracks = prof_->GetBinContent(2);
    const double numTracksLost = prof_->GetBinContent(3);
    const double totalTracks = prof_->GetBinContent(5);
    const double numTracksMatched = prof_->GetBinContent(4);
    const double numTPsAll = prof_->GetBinContent(6);
    const double numTPsEff = prof_->GetBinContent(7);
    const double numTPsLost = prof_->GetBinContent(8);
    const double numTPsEffPerfect = prof_->GetBinContent(10);
    const double errStubs = prof_->GetBinError(1);
    const double errTracks = prof_->GetBinError(2);
    const double errTracksLost = prof_->GetBinError(3);
    const double fracFake = (totalTracks - numTracksMatched) / totalTracks;
    const double fracDup = (numTracksMatched - numTPsAll) / totalTracks;
    const double eff = numTPsEff / totalTPs;
    const double errEff = sqrt(eff * (1. - eff) / totalTPs / nEvents_);
    const double effLoss = numTPsLost / totalTPs;
    const double errEffLoss = sqrt(effLoss * (1. - effLoss) / totalTPs / nEvents_);
    const double effPerfect = numTPsEffPerfect / totalTPs;
    const double errEffPerfect = sqrt(effPerfect * (1. - effPerfect) / totalTPs / nEvents_);
    const vector<double> nums = {numStubs, numTracks, numTracksLost};
    const vector<double> errs = {errStubs, errTracks, errTracksLost};
    const int wNums = ceil(log10(*max_element(nums.begin(), nums.end()))) + 5;
    const int wErrs = ceil(log10(*max_element(errs.begin(), errs.end()))) + 5;
    log_ << "                        KFin  SUMMARY                        " << endl;
    log_ << "number of stubs       per TFP = " << setw(wNums) << numStubs << " +- " << setw(wErrs) << errStubs << endl;
    log_ << "number of tracks      per TFP = " << setw(wNums) << numTracks << " +- " << setw(wErrs) << errTracks
         << endl;
    log_ << "number of lost tracks per TFP = " << setw(wNums) << numTracksLost << " +- " << setw(wErrs) << errTracksLost
         << endl;
    log_ << "  current tracking efficiency = " << setw(wNums) << effPerfect << " +- " << setw(wErrs) << errEffPerfect
         << endl;
    log_ << "  max     tracking efficiency = " << setw(wNums) << eff << " +- " << setw(wErrs) << errEff << endl;
    log_ << "     lost tracking efficiency = " << setw(wNums) << effLoss << " +- " << setw(wErrs) << errEffLoss << endl;
    log_ << "                    fake rate = " << setw(wNums) << fracFake << endl;
    log_ << "               duplicate rate = " << setw(wNums) << fracDup << endl;
    log_ << "=============================================================";
    LogPrint("L1Trigger/TrackerTFP") << log_.str();
  }

  //
  void AnalyzerKFin::formTracks(const StreamsTrack& streamsTrack,
                                const StreamsStub& streamsStubs,
                                vector<vector<TTStubRef>>& tracks,
                                int channel) const {
    const int offset = channel * setup_->numLayers();
    const StreamTrack& streamTrack = streamsTrack[channel];
    const int numTracks = accumulate(streamTrack.begin(), streamTrack.end(), 0, [](int sum, const FrameTrack& frame) {
      return sum + (frame.first.isNonnull() ? 1 : 0);
    });
    tracks.reserve(numTracks);
    for (int frame = 0; frame < (int)streamTrack.size(); frame++) {
      const FrameTrack& frameTrack = streamTrack[frame];
      if (frameTrack.first.isNull())
        continue;
      vector<TTStubRef> ttStubRefs;
      ttStubRefs.reserve(setup_->numLayers());
      for (int layer = 0; layer < setup_->numLayers(); layer++) {
        const FrameStub& stub = streamsStubs[offset + layer][frame];
        if (stub.first.isNonnull())
          ttStubRefs.push_back(stub.first);
      }
      tracks.push_back(ttStubRefs);
    }
  }

  //
  void AnalyzerKFin::associate(const vector<vector<TTStubRef>>& tracks,
                               const StubAssociation* ass,
                               set<TPPtr>& tps,
                               int& sum,
                               bool perfect) const {
    for (const vector<TTStubRef>& ttStubRefs : tracks) {
      const vector<TPPtr>& tpPtrs = perfect ? ass->associateFinal(ttStubRefs) : ass->associate(ttStubRefs);
      if (tpPtrs.empty())
        continue;
      sum++;
      copy(tpPtrs.begin(), tpPtrs.end(), inserter(tps, tps.begin()));
    }
  }

}  // namespace trklet

DEFINE_FWK_MODULE(trklet::AnalyzerKFin);
