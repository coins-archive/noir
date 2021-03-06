// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef NOIRNODEMAN_H
#define NOIRNODEMAN_H

#include "noirnode.h"
#include "sync.h"

using namespace std;

class CNoirnodeMan;

extern CNoirnodeMan mnodeman;

/**
 * Provides a forward and reverse index between MN vin's and integers.
 *
 * This mapping is normally add-only and is expected to be permanent
 * It is only rebuilt if the size of the index exceeds the expected maximum number
 * of MN's and the current number of known MN's.
 *
 * The external interface to this index is provided via delegation by CNoirnodeMan
 */
class CNoirnodeIndex
{
public: // Types
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

    typedef std::map<int,CTxIn> rindex_m_t;

    typedef rindex_m_t::iterator rindex_m_it;

    typedef rindex_m_t::const_iterator rindex_m_cit;

private:
    int                  nSize;

    index_m_t            mapIndex;

    rindex_m_t           mapReverseIndex;

public:
    CNoirnodeIndex();

    int GetSize() const {
        return nSize;
    }

    /// Retrieve noirnode vin by index
    bool Get(int nIndex, CTxIn& vinNoirnode) const;

    /// Get index of a noirnode vin
    int GetNoirnodeIndex(const CTxIn& vinNoirnode) const;

    void AddNoirnodeVIN(const CTxIn& vinNoirnode);

    void Clear();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapIndex);
        if(ser_action.ForRead()) {
            RebuildIndex();
        }
    }

private:
    void RebuildIndex();

};

class CNoirnodeMan
{
public:
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

private:
    static const int MAX_EXPECTED_INDEX_SIZE = 30000;

    /// Only allow 1 index rebuild per hour
    static const int64_t MIN_INDEX_REBUILD_TIME = 3600;

    static const std::string SERIALIZATION_VERSION_STRING;

    static const int DSEG_UPDATE_SECONDS        = 3 * 60 * 60;

    static const int LAST_PAID_SCAN_BLOCKS      = 100;

    static const int MIN_POSE_PROTO_VERSION     = 70203;
    static const int MAX_POSE_CONNECTIONS       = 10;
    static const int MAX_POSE_RANK              = 10;
    static const int MAX_POSE_BLOCKS            = 10;

    static const int MNB_RECOVERY_QUORUM_TOTAL      = 10;
    static const int MNB_RECOVERY_QUORUM_REQUIRED   = 6;
    static const int MNB_RECOVERY_MAX_ASK_ENTRIES   = 10;
    static const int MNB_RECOVERY_WAIT_SECONDS      = 60;
    static const int MNB_RECOVERY_RETRY_SECONDS     = 3 * 60 * 60;


    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    // map to hold all MNs
    std::vector<CNoirnode> vNoirnodes;
    // who's asked for the Noirnode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForNoirnodeList;
    // who we asked for the Noirnode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForNoirnodeList;
    // which Noirnodes we've asked for
    std::map<COutPoint, std::map<CNetAddr, int64_t> > mWeAskedForNoirnodeListEntry;
    // who we asked for the noirnode verification
    std::map<CNetAddr, CNoirnodeVerification> mWeAskedForVerification;

    // these maps are used for noirnode recovery from NOIRNODE_NEW_START_REQUIRED state
    std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > > mMnbRecoveryRequests;
    std::map<uint256, std::vector<CNoirnodeBroadcast> > mMnbRecoveryGoodReplies;
    std::list< std::pair<CService, uint256> > listScheduledMnbRequestConnections;

    int64_t nLastIndexRebuildTime;

    CNoirnodeIndex indexNoirnodes;

    CNoirnodeIndex indexNoirnodesOld;

    /// Set when index has been rebuilt, clear when read
    bool fIndexRebuilt;

    /// Set when noirnodes are added, cleared when CGovernanceManager is notified
    bool fNoirnodesAdded;

    /// Set when noirnodes are removed, cleared when CGovernanceManager is notified
    bool fNoirnodesRemoved;

    std::vector<uint256> vecDirtyGovernanceObjectHashes;

    int64_t nLastWatchdogVoteTime;

    friend class CNoirnodeSync;

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, std::pair<int64_t, CNoirnodeBroadcast> > mapSeenNoirnodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CNoirnodePing> mapSeenNoirnodePing;
    // Keep track of all verifications I've seen
    std::map<uint256, CNoirnodeVerification> mapSeenNoirnodeVerification;
    // keep track of dsq count to prevent noirnodes from gaming darksend queue
    int64_t nDsqCount;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        LOCK(cs);
        std::string strVersion;
        if(ser_action.ForRead()) {
            READWRITE(strVersion);
        }
        else {
            strVersion = SERIALIZATION_VERSION_STRING; 
            READWRITE(strVersion);
        }

        READWRITE(vNoirnodes);
        READWRITE(mAskedUsForNoirnodeList);
        READWRITE(mWeAskedForNoirnodeList);
        READWRITE(mWeAskedForNoirnodeListEntry);
        READWRITE(mMnbRecoveryRequests);
        READWRITE(mMnbRecoveryGoodReplies);
        READWRITE(nLastWatchdogVoteTime);
        READWRITE(nDsqCount);

        READWRITE(mapSeenNoirnodeBroadcast);
        READWRITE(mapSeenNoirnodePing);
        READWRITE(indexNoirnodes);
        if(ser_action.ForRead() && (strVersion != SERIALIZATION_VERSION_STRING)) {
            Clear();
        }
    }

    CNoirnodeMan();

    /// Add an entry
    bool Add(CNoirnode &mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode *pnode, const CTxIn &vin);
    void AskForMnb(CNode *pnode, const uint256 &hash);

    /// Check all Noirnodes
    void Check();

    /// Check all Noirnodes and remove inactive
    void CheckAndRemove();

    /// Clear Noirnode vector
    void Clear();

    /// Count Noirnodes filtered by nProtocolVersion.
    /// Noirnode nProtocolVersion should match or be above the one specified in param here.
    int CountNoirnodes(int nProtocolVersion = -1);
    /// Count enabled Noirnodes filtered by nProtocolVersion.
    /// Noirnode nProtocolVersion should match or be above the one specified in param here.
    int CountEnabled(int nProtocolVersion = -1);

    /// Count Noirnodes by network type - NET_IPV4, NET_IPV6, NET_TOR
    // int CountByIP(int nNetworkType);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CNoirnode* Find(const CScript &payee);
    CNoirnode* Find(const CTxIn& vin);
    CNoirnode* Find(const CPubKey& pubKeyNoirnode);

    /// Versions of Find that are safe to use from outside the class
    bool Get(const CPubKey& pubKeyNoirnode, CNoirnode& noirnode);
    bool Get(const CTxIn& vin, CNoirnode& noirnode);

    /// Retrieve noirnode vin by index
    bool Get(int nIndex, CTxIn& vinNoirnode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexNoirnodes.Get(nIndex, vinNoirnode);
    }

    bool GetIndexRebuiltFlag() {
        LOCK(cs);
        return fIndexRebuilt;
    }

    /// Get index of a noirnode vin
    int GetNoirnodeIndex(const CTxIn& vinNoirnode) {
        LOCK(cs);
        return indexNoirnodes.GetNoirnodeIndex(vinNoirnode);
    }

    /// Get old index of a noirnode vin
    int GetNoirnodeIndexOld(const CTxIn& vinNoirnode) {
        LOCK(cs);
        return indexNoirnodesOld.GetNoirnodeIndex(vinNoirnode);
    }

    /// Get noirnode VIN for an old index value
    bool GetNoirnodeVinForIndexOld(int nNoirnodeIndex, CTxIn& vinNoirnodeOut) {
        LOCK(cs);
        return indexNoirnodesOld.Get(nNoirnodeIndex, vinNoirnodeOut);
    }

    /// Get index of a noirnode vin, returning rebuild flag
    int GetNoirnodeIndex(const CTxIn& vinNoirnode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexNoirnodes.GetNoirnodeIndex(vinNoirnode);
    }

    void ClearOldNoirnodeIndex() {
        LOCK(cs);
        indexNoirnodesOld.Clear();
        fIndexRebuilt = false;
    }

    bool Has(const CTxIn& vin);

    noirnode_info_t GetNoirnodeInfo(const CTxIn& vin);

    noirnode_info_t GetNoirnodeInfo(const CPubKey& pubKeyNoirnode);

    char* GetNotQualifyReason(CNoirnode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount);

    /// Find an entry in the noirnode list that is next to be paid
    CNoirnode* GetNextNoirnodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);
    /// Same as above but use current block height
    CNoirnode* GetNextNoirnodeInQueueForPayment(bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CNoirnode* FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion = -1);

    std::vector<CNoirnode> GetFullNoirnodeVector() { return vNoirnodes; }

    std::vector<std::pair<int, CNoirnode> > GetNoirnodeRanks(int nBlockHeight = -1, int nMinProtocol=0);
    int GetNoirnodeRank(const CTxIn &vin, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);
    CNoirnode* GetNoirnodeByRank(int nRank, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);

    void ProcessNoirnodeConnections();
    std::pair<CService, std::set<uint256> > PopScheduledMnbRequestConnection();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void DoFullVerificationStep();
    void CheckSameAddr();
    bool SendVerifyRequest(const CAddress& addr, const std::vector<CNoirnode*>& vSortedByAddr);
    void SendVerifyReply(CNode* pnode, CNoirnodeVerification& mnv);
    void ProcessVerifyReply(CNode* pnode, CNoirnodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CNoirnodeVerification& mnv);

    /// Return the number of (unique) Noirnodes
    int size() { return vNoirnodes.size(); }

    std::string ToString() const;

    /// Update noirnode list and maps using provided CNoirnodeBroadcast
    void UpdateNoirnodeList(CNoirnodeBroadcast mnb);
    /// Perform complete check and only then update list and maps
    bool CheckMnbAndUpdateNoirnodeList(CNode* pfrom, CNoirnodeBroadcast mnb, int& nDos);
    bool IsMnbRecoveryRequested(const uint256& hash) { return mMnbRecoveryRequests.count(hash); }

    void UpdateLastPaid();

    void CheckAndRebuildNoirnodeIndex();

    void AddDirtyGovernanceObjectHash(const uint256& nHash)
    {
        LOCK(cs);
        vecDirtyGovernanceObjectHashes.push_back(nHash);
    }

    std::vector<uint256> GetAndClearDirtyGovernanceObjectHashes()
    {
        LOCK(cs);
        std::vector<uint256> vecTmp = vecDirtyGovernanceObjectHashes;
        vecDirtyGovernanceObjectHashes.clear();
        return vecTmp;;
    }

    bool IsWatchdogActive();
    void UpdateWatchdogVoteTime(const CTxIn& vin);
    bool AddGovernanceVote(const CTxIn& vin, uint256 nGovernanceObjectHash);
    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void CheckNoirnode(const CTxIn& vin, bool fForce = false);
    void CheckNoirnode(const CPubKey& pubKeyNoirnode, bool fForce = false);

    int GetNoirnodeState(const CTxIn& vin);
    int GetNoirnodeState(const CPubKey& pubKeyNoirnode);

    bool IsNoirnodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt = -1);
    void SetNoirnodeLastPing(const CTxIn& vin, const CNoirnodePing& mnp);

    void UpdatedBlockTip(const CBlockIndex *pindex);

    /**
     * Called to notify CGovernanceManager that the noirnode index has been updated.
     * Must be called while not holding the CNoirnodeMan::cs mutex
     */
    void NotifyNoirnodeUpdates();

};

#endif
