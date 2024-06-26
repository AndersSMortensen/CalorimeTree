#include "Utilities.h"
#include "Cuts.h"
#include "PhysicsObjects.h"
#include "TreeUtilities.h"
#include "HistogramLibrary.h"
#include "ClusterECorrections.h"

void makeHistosFromTree(TString AnalysisDirectory, int jobId = 0)
{
  ENTER

  if (jobId < 0)
    FATAL("Negative jobId")
  if(!jobId)
    LOG("This is a debug run")

  GlobalOptions optns(AnalysisDirectory, jobId);

  EventCuts eventCuts(optns);
  IsoGammaCuts isoGammaCuts(optns);
  JetCuts jetCuts(optns);
  Pi0Cuts pi0Cuts(optns);

  TFile *fOut = new TFile(Form("%s/HistosFromTree_%d.root", AnalysisDirectory.Data(), jobId), "RECREATE");

  TDirectory *hDirEvents = DefineEventHistograms(fOut, optns);
  TDirectory *hQADirEvents = DefineEventQAHistograms(fOut, optns);
  TDirectory *hDirIsoGammas = DefineIsoGammaHistograms(fOut, optns);
  TDirectory *hQADirIsoGammas = DefineIsoGammaQAHistograms(fOut, optns);
  TDirectory *hDirJets = DefineJetHistograms(fOut, optns);
  TDirectory *hQADirJets = DefineJetQAHistograms(fOut, optns);

  // These vectors store all information about all selected (by cuts) physics objects within a given event
  std::vector<IsoGamma> IsoGammas;
  std::vector<Pi0> Pi0sForIsoGammaQA;
  std::vector<Jet> Jets;
  std::vector<PLJet> PLJets; // Particle Level Jets -> Will only be filled if this is a MC
  std::vector<Pi0> Pi0s;

  TChain *chain = readTree(Form("%s/../InputFiles/InputFiles_group_%d.txt", AnalysisDirectory.Data(), jobId));

  optns.TreeFormat = listTreeBranches(chain);

  TreeBuffer tree(chain, optns);

  for (int iEvent = 0; iEvent < tree.NEvents; iEvent++)
  {
    chain->GetEntry(iEvent);

    if (jobId == 0)
      PrintProgress(iEvent, tree.NEvents);
    else
      PrintProgressNumber(iEvent, tree.NEvents, 1000);

    Event event(tree, optns);

    if (!eventCuts.PassedCuts(event))
      continue;
    fillHistograms(event, hDirEvents, event.weight);
    if (optns.doQA)
      fillQAHistograms(event, hQADirEvents, event.weight, optns);

    if (optns.doIsoGamma)
    {
      saveClustersFromEventInVector(tree, IsoGammas, optns);
      if (isoGammaCuts.applyNonLin)
        applyNonLinAndFineTuningCorrection(IsoGammas, isoGammaCuts, optns);
      pairIsoGammasFromEventInVector(IsoGammas, Pi0sForIsoGammaQA);
      calculateIsolation(IsoGammas, event, isoGammaCuts.useRhoInsteadOfPerpCone);
      doIsoGammaCuts(IsoGammas, isoGammaCuts);
      fillHistograms(IsoGammas, hDirIsoGammas, event.weight);
      if (optns.doQA)
      {
        fillQAHistograms(IsoGammas, hQADirIsoGammas, event.weight, optns);
        fillQAHistograms(Pi0sForIsoGammaQA, hQADirIsoGammas, event.weight, optns);
      }
    }
    if (optns.doJets)
    {
      saveJetsFromEventInVector(tree, Jets);
      // TODO: JetCuts
      fillHistograms(Jets, hDirJets, event.weight);
      if (optns.doQA)
        fillQAHistograms(Jets, hQADirJets, event.weight, optns);
      if (optns.isMC)
      {
        savePLJetsFromEventInVector(tree, PLJets);
        mapPLtoDLjets(Jets, PLJets, jetCuts.R);
        // TODO: Do I need PL jet cuts?
        fillHistograms(PLJets, hDirJets, event.weight);
        if (optns.doQA)
          fillQAHistograms(PLJets, hQADirJets, event.weight, optns);
      }
    }
    if (optns.doIsoGamma && optns.doJets)
    {
      // TODO: Correlations
    }
    IsoGammas.clear();
    Jets.clear();
    PLJets.clear();
    Pi0s.clear();
    Pi0sForIsoGammaQA.clear();
  }

  fOut->Write();

  EXIT
}