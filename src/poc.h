// Copyright (c) 2016 The FairCoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POC_H
#define BITCOIN_POC_H

#include "consensus/params.h"
#include "primitives/block.h"
#include "chainparams.h"
#include "chain.h"
#include "sync.h"

#include <stdint.h>
#include <boost/unordered_set.hpp>
#include <boost/filesystem.hpp>

#define GENESIS_NODE_ID  0xc001d00d
#define GENESIS_ADMIN_ID 0xad3aee01

typedef std::map<uint32_t, CCvnInfo> CvnMapType;
typedef std::map<uint32_t, CChainAdmin> ChainAdminMapType;
typedef std::map<uint32_t, CNoncePool> CNoncePoolType;
typedef std::map<uint256, CChainDataMsg> ChainDataMapType;

typedef std::map<uint32_t, CCvnPartialSignature> MapSigSigner;
typedef std::map<vector<uint32_t>, MapSigSigner> MapMissing;
typedef std::map<uint32_t, MapMissing> MapSigCreator;
typedef std::map<uint256, MapSigCreator> MapSigTip;

typedef boost::unordered_set<uint32_t> TimeWeightSetType;
typedef std::vector<uint32_t>::reverse_iterator CandidateIterator;
typedef map<uint256, const CBlockIndex *> BlockIndexByPrevHashType;
typedef map<uint32_t, uint32_t> BannedCVNMapType; // nCreatorID/nHeight at which it has been banned

/** dynamic chain parameters range checks */
#define MAX_BLOCK_SPACING 3600
#define MIN_BLOCK_SPACING 30
#define MAX_DUST_THRESHOLD 1 * COIN
#define MIN_DUST_THRESHOLD 0
#define MAX_TX_FEE_THRESHOLD 1 * COIN
#define MIN_TX_FEE_THRESHOLD 0
#define MIN_BLOCKS_TO_CONSIDER_FOR_SIG_CHECK 1
#define MAX_BLOCKS_TO_CONSIDER_FOR_SIG_CHECK 100
#define MIN_PERCENTAGE_OF_SIGNATURES_MEAN 33
#define MAX_PERCENTAGE_OF_SIGNATURES_MEAN 100
#define MIN_SIZE_OF_BLOCK 100000
#define MAX_SIZE_OF_BLOCK 5000000
#define NONCE_POOL_WAIT_TIME 20

#define DEFAULT_NONCE_POOL_SIZE 20
#define MAX_NONCE_POOL_SIZE 100

#define __DBG_ LogPrintf("DEBUG: In file %s in function %s in line %d\n", __FILE__, __func__, __LINE__);

extern uint32_t nCvnNodeId;
extern uint32_t nChainAdminId;

extern const char *pocStateNames[];

extern CCriticalSection cs_mapCVNs;
extern CvnMapType mapCVNs;
extern CCriticalSection cs_mapChainAdmins;
extern ChainAdminMapType mapChainAdmins;
extern CCriticalSection cs_mapNoncePool;
extern CNoncePoolType mapNoncePool;
extern CCriticalSection cs_mapChainData;
extern ChainDataMapType mapChainData;
extern CCriticalSection cs_mapBlockIndexByPrevHash;
extern BlockIndexByPrevHashType mapBlockIndexByPrevHash;
extern CCriticalSection cs_mapBannedCVNs;
extern BannedCVNMapType mapBannedCVNs;

enum POCState {
    INIT,
    NONCE_POOL_CHANGES,
    CREATE_SIGNATURE,
    WAITING_FOR_SIGNATURES,
    WAITING_FOR_BLOCK,
    WAITING_FOR_NEW_TIP,
    WAITING_FOR_CVN_DATA,
};

class POCStateHolder {
public:
    POCState state;

    uint32_t nNodeId;
    uint32_t nNextCreator;
    uint32_t nLastCreator;
    uint32_t nSleep;

    vector<uint32_t> vMissingSignatures;

    CBlockIndex *pindexLastTip, *pindexPrev;
    const CChainParams& chainparams;
    CReserveScript& feeScript;

    POCStateHolder(const POCState stateIn, const uint32_t nNextCreatorIn, CBlockIndex *pindexPrevIn, const uint32_t nNodeIdIn, const CChainParams& chainparamsIn, CReserveScript& feeScriptIn)
            : chainparams(chainparamsIn), feeScript(feeScriptIn)
    {
        state         = stateIn;
        nNodeId       = nNodeIdIn;
        nNextCreator  = nNextCreatorIn;
        nLastCreator  = nNextCreatorIn;
        nSleep        = 0;

        pindexPrev    = pindexPrevIn;
        pindexLastTip = pindexPrevIn;
        vMissingSignatures.clear();
    }

    void Reset(uint32_t nNextCreatorIn, CBlockIndex *pindexPrevIn)
    {
        state         = NONCE_POOL_CHANGES;
        nLastCreator  = nNextCreator;
        nNextCreator  = nNextCreatorIn;
        pindexLastTip = pindexPrev;
        pindexPrev    = pindexPrevIn;
        nSleep        = 0;
        vMissingSignatures.clear();
    }

    bool NewTip() const
    {
        return pindexLastTip != pindexPrev;
    }

    bool BlockSpacingTimeout() const
    {
        return nLastCreator != nNextCreator;
    }

    const uint256 GetPrevBlockHash() const
    {
        return pindexPrev->GetBlockHash();
    }
};

class CSignatureHolder
{
private:

public:
    MapSigTip mapTip;     // map per block hash
    CCriticalSection cs_mapCvnSigs;

    CSignatureHolder()
    {
        SetNull();
    }

    void SetNull()
    {
        LOCK(cs_mapCvnSigs);
        mapTip.clear();
    }

    void AddSig(const CCvnPartialSignature &sig);
    bool GetSignatureSet(MapSigSigner &sigs, const vector<uint32_t> &vMissingSignerIds, const uint256 &hashPrevBlock, const uint32_t nNextCreator);
    bool GetSignature(CCvnPartialSignature &sig, const uint256 &hashPrevBlock, const uint32_t nNextCreator, const uint32_t nSignerId, const vector<uint32_t> &vMissingSignerIds);
    bool GetSignatures(MapSigSigner &sigs, const uint256 &hashPrevBlock, const uint32_t nNextCreator);
    bool GetMissing(MapMissing &missing, const uint256 &hashPrevBlock, const uint32_t nNextCreator);

    std::string ToString();
};

extern CSignatureHolder sigHolder;

extern bool CvnSignHash(const uint256 &hashToSign, CSchnorrSig& signature);
extern bool CvnSignPartial(const uint256 &hashPrevBlock, CCvnPartialSignatureUnsinged &signature, const uint32_t &nNextCreator, const uint32_t &nNodeId, const vector<uint32_t> &vMissingCvnIds);
extern int CombinePartialSignatures(CSchnorrSig& allsig, uint8_t *sigs[], int nSignatures);
extern bool CvnSignBlock(CBlock& block);
extern bool CvnVerifyChainSignature(const CBlockHeader& block);
extern bool CvnVerifySignature(const uint256 &hash, const CSchnorrSig &sig, const CSchnorrPubKey &pubKey);
extern bool CvnVerifySignature(const uint256 &hash, const CSchnorrSig &sig, const uint32_t nCvnId);
extern bool CvnVerifyAdminSignature(const vector<uint32_t> &nAdminIds, const uint256 &hashAdmin, const CSchnorrSig &sig);
extern bool CheckForDuplicateCvns(const CBlock& block);
extern bool CheckForDuplicateChainAdmins(const CBlock& block);
extern bool CheckForDuplicateAdminSigs(const CBlock& block);
extern bool CheckForDuplicateMissingChainSigs(const CBlock& block);
extern bool SendCVNSignature(const CBlockIndex *pindexNew);
extern bool AddCvnSignature(const CCvnPartialSignature& msg);
extern bool AddChainData(const CChainDataMsg& msg);
extern bool CvnVerifyPartialSignature(const CCvnPartialSignature &sig);
extern bool CvnVerifyPartialSignature(const uint256 &hash, const CSchnorrSig &sig, const CSchnorrPubKey &pubKey, const CSchnorrPubKey &sumPublicNoncesOthers);
extern bool CheckAdminSignature(const vector<uint32_t> &vAdminIds, const uint256 &hashAdmin, const CSchnorrSig &sig, const bool fCoinSupply);
extern void RelayChainData(const CChainDataMsg& msg);
extern void RelayCvnSignature(const CCvnPartialSignature& msg);

extern bool AddNoncePool(CNoncePool& msg);
extern void CreateNewNoncePool(const POCStateHolder& s);
extern void RelayNoncePool(const CNoncePool& msg);
extern void RemoveCvnPubNonces(const uint256& hashPrevBlock);

extern uint32_t CheckNextBlockCreator(const CBlockIndex* pindexStart, const int64_t nTimeToTest, CCvnStatus* state = NULL);

/** Check whether a block hash satisfies the proof-of-cooperation requirements */
extern bool CheckProofOfCooperation(const CBlockHeader& block, const Consensus::Params&);

extern void UpdateCvnInfo(const CBlock* pblock);
extern void UpdateChainParameters(const CBlock* pblock);
extern void UpdateChainAdmins(const CBlock* pblock);

extern bool CheckDynamicChainParameters(const CDynamicChainParams& params);

extern void POC_create_secp256k1_context();
extern void POC_destroy_secp256k1_context();

/** start the proof-of-cooperation thread */
extern void RunPOCThread(const bool fGenerate, const CChainParams& chainparams, const uint32_t& nNodeId);



/** Access to the nonces pool database (pool.dat) */
class CNoncesPoolDB
{
private:
    boost::filesystem::path pathNonces;
public:
    CNoncesPoolDB();
    bool Write(const CNoncePool& pool, const vector<CSchnorrPrivNonce>& vPrivateNonces);
    bool Read(CNoncePool& pool, vector<CSchnorrPrivNonce>& vPrivateNonces);
};

void SaveNoncesPool();

#endif // BITCOIN_POC_H
