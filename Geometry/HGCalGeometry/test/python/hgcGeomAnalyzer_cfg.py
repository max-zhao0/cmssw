#run with: cmsRun hgcGeomAnalyzer_cfg.py geom=v10

import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing
from Configuration.StandardSequences.Eras import eras

options = VarParsing()
options.register ("geom", "",  VarParsing.multiplicity.singleton, VarParsing.varType.string)
options.parseArguments()

fileName = "geom_output_"+options.geom

process = cms.Process("demo",eras.Phase2C8)

process.load("Configuration.StandardSequences.FrontierConditions_GlobalTag_cff")
if options.geom == 'v10':
    process.load('Configuration.Geometry.GeometryExtended2026D41Reco_cff')
elif options.geom == 'v11':
    process.load('Configuration.Geometry.GeometryExtended2026D46Reco_cff')
else:
    raise Exception('UNKNOWN GEOMETRY!')

from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, 'auto:phase2_realistic', '')

process.maxEvents = cms.untracked.PSet( input = cms.untracked.int32(1) )
process.source = cms.Source("EmptySource")

process.plotter = cms.EDAnalyzer("HGCGeomAnalyzer",
    fileName = cms.string(fileName+".txt")
    )

process.TFileService = cms.Service("TFileService",
                                   fileName = cms.string(fileName+".root")
)

process.p = cms.Path(process.plotter)
