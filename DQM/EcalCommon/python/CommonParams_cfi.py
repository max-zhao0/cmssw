import FWCore.ParameterSet.Config as cms

ecalCommonParams = cms.untracked.PSet(
    onlineMode = cms.untracked.bool(False),
    willConvertToEDM = cms.untracked.bool(True)
)

ecaldqmLaserWavelengths = [1, 2, 3]
ecaldqmLedWavelengths = [1, 2]
ecaldqmMGPAGains = [12]
ecaldqmMGPAGainsPN = [16]
ecaldqmLedWavelengths = cms.untracked.vint32(1, 2)
ecaldqmMGPAGains = cms.untracked.vint32(12)
ecaldqmMGPAGainsPN = cms.untracked.vint32(16)
