#ifndef CommonTools_ParticleFlow_PFnoPileUpPacked_
#define CommonTools_ParticleFlow_PFnoPileUpPacked_

// system include files
#include <memory>
#include <string>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "DataFormats/Candidate/interface/Candidate.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"
#include "DataFormats/Common/interface/Association.h"

#include "DataFormats/VertexReco/interface/Vertex.h"

#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Framework/interface/EventSetup.h"

#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

using namespace std;
using namespace edm;
using namespace reco;


/**\class PFnoPileUpPacked
\brief Identifies pile-up candidates from a collection of Candidates, and
produces the corresponding collection of NoPileUpCandidates.

\author Andreas Hinzmann
\date   May 2021

*/

class PFnoPileUpPacked : public edm::stream::EDProducer<> {
public:
  typedef edm::View<reco::Candidate> CandidateView;
  typedef edm::Association<reco::VertexCollection> CandToVertex;

  explicit PFnoPileUpPacked(const edm::ParameterSet&);

  ~PFnoPileUpPacked() override = default;

  void produce(edm::Event&, const edm::EventSetup&) override;

  static void fillDescriptions(edm::ConfigurationDescriptions & descriptions);

private:
  edm::EDGetTokenT<CandidateView> tokenCandidatesView_;
  edm::EDGetTokenT<reco::VertexCollection> tokenVertices_;
  edm::EDGetTokenT<CandToVertex> tokenVertexAssociation_;
  edm::EDGetTokenT<edm::ValueMap<int>> tokenVertexAssociationQuality_;
  int vertexAssociationQuality_;
};


PFnoPileUpPacked::PFnoPileUpPacked(const edm::ParameterSet& iConfig) {
  tokenCandidatesView_ = consumes<CandidateView>(iConfig.getParameter<InputTag>("candidates"));
  vertexAssociationQuality_ = iConfig.getParameter<int>("vertexAssociationQuality");
  tokenVertexAssociation_ = consumes<CandToVertex>(iConfig.getParameter<edm::InputTag>("vertexAssociation"));
  tokenVertexAssociationQuality_ =
      consumes<edm::ValueMap<int>>(iConfig.getParameter<edm::InputTag>("vertexAssociation"));
  produces<edm::PtrVector<reco::Candidate>>();
}

void PFnoPileUpPacked::produce(Event& iEvent, const EventSetup& iSetup) {
  unique_ptr<edm::PtrVector<reco::Candidate>> pOutput(new edm::PtrVector<reco::Candidate>);
  Handle<CandidateView> candidateView;
  iEvent.getByToken(tokenCandidatesView_, candidateView);
  const edm::Association<reco::VertexCollection> associatedPV = iEvent.get(tokenVertexAssociation_);
  const edm::ValueMap<int> associationQuality = iEvent.get(tokenVertexAssociationQuality_);
  for (auto p : candidateView->ptrs()) {
    const reco::VertexRef& PVOrig = associatedPV[p];
    int quality = associationQuality[p];
    if (!(PVOrig.isNonnull() && (PVOrig.key() > 0) && (quality >= vertexAssociationQuality_)))
      pOutput->push_back(p);
  }
  iEvent.put(std::move(pOutput));
}

void PFnoPileUpPacked::fillDescriptions(edm::ConfigurationDescriptions & descriptions) {
   edm::ParameterSetDescription desc;
   desc.add<edm::InputTag>("candidates", edm::InputTag("packedPFCandidates"));
   desc.add<int>("vertexAssociationQuality", 7);
   desc.add<edm::InputTag>("vertexAssociation", edm::InputTag("packedPrimaryVertexAssociationJME","original"));
   descriptions.add("produceMuons", desc);
}

DEFINE_FWK_MODULE(PFnoPileUpPacked);
#endif