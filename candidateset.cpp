/*
 * candidateset.cpp
 *
 *  Created on: Jun 1, 2014
 *      Author: Tung Nguyen
 */

#include "phylotree.h"
#include "candidateset.h"
extern Params *globalParam;

CandidateSet::CandidateSet(int limit, int max_candidates, Alignment *aln) : CheckpointFactory() {
    assert(max_candidates <= limit);
    assert(aln);
    this->maxCandidates = limit;
    this->popSize = max_candidates;
    this->aln = aln;
    this->bestScore = -DBL_MAX;
}

CandidateSet::CandidateSet() : CheckpointFactory() {
	aln = NULL;
	maxCandidates = 0;
	popSize = 0;
	bestScore = -DBL_MAX;
}

void CandidateSet::saveCheckpoint() {
    checkpoint->startStruct("CandidateSet");
    int ntrees = (int) size();
    assert(ntrees <= globalParam->numParsTrees);
    checkpoint->startList(ntrees);
    for (reverse_iterator it = rbegin(); it != rend() && ntrees > 0; it++, ntrees--) {
        checkpoint->addListElement();
        stringstream ss;
        ss.precision(12);
        ss << it->second.score << " " << it->second.tree;
//        double score = it->second.score;
//        CKP_SAVE(score);
//        checkpoint->put("tree", it->second.tree);
        checkpoint->put("", ss.str());
    }
    checkpoint->endList();
    checkpoint->endStruct();
    CheckpointFactory::saveCheckpoint();
}

void CandidateSet::restoreCheckpoint() {
    CheckpointFactory::restoreCheckpoint();
    checkpoint->startStruct("CandidateSet");
    double score;
    string tree;
    checkpoint->startList(globalParam->numParsTrees);
    for (int i = 0; i < globalParam->numParsTrees; i++) {
        checkpoint->addListElement();
        string str;
        if (!checkpoint->getString("", str)) {
            break;
        }
        stringstream ss(str);
        ss >> score >> tree;
//        CKP_RESTORE(tree);
        update(tree, score);

    }
    checkpoint->endList();
    checkpoint->endStruct();
}

vector<string> CandidateSet::getEquallyOptimalTrees() {
	vector<string> res;
	for (reverse_iterator rit = rbegin(); rit != rend() && rit->second.score == bestScore; rit++) {
		res.push_back(rit->second.tree);
	}
	return res;
}

string CandidateSet::getRandCandTree() {
	assert(!empty());
	if (empty())
		return "";
	int id = random_int(min(popSize, (int)size()) );
	for (reverse_iterator i = rbegin(); i != rend(); i++, id--)
		if (id == 0)
			return i->second.tree;
	assert(0);
	return "";
}

vector<string> CandidateSet::getHighestScoringTrees(int numTree) {
	assert(numTree <= maxCandidates);
	if (numTree == 0) {
		numTree = maxCandidates;
	}
	vector<string> res;
	int cnt = numTree;
	for (reverse_iterator rit = rbegin(); rit != rend() && cnt > 0; rit++, cnt--) {
		res.push_back(rit->second.tree);
	}
	return res;
}

bool CandidateSet::replaceTree(string tree, double score) {
    CandidateTree candidate;
    candidate.tree = tree;
    candidate.score = score;
    candidate.topology = getTopology(tree);
    if (treeTopologyExist(candidate.topology)) {
        topologies[candidate.topology] = score;
        for (reverse_iterator i = rbegin(); i != rend(); i++) {
            if (i->second.topology == candidate.topology) {
                erase( --(i.base()) );
                break;
            }
            insert(CandidateSet::value_type(score, candidate));
        }
    } else {
        return false;
    }
    return true;
}

string CandidateSet::getNextCandTree() {
    string tree;
    assert(!empty());
    if (parentTrees.empty()) {
        initParentTrees();
    }
    tree = parentTrees.top();
    parentTrees.pop();
    return tree;
}

void CandidateSet::initParentTrees() {
    if (parentTrees.empty()) {
        int count = this->popSize;
        for (reverse_iterator i = rbegin(); i != rend() && count >0 ; i++, count--) {
            parentTrees.push(i->second.tree);
            //cout << i->first << endl;
        }
    }
}

bool CandidateSet::update(string tree, double score) {
	bool newTree;
	CandidateTree candidate;
	candidate.tree = tree;
	candidate.score = score;
	candidate.topology = getTopology(tree);
	if (candidate.score > bestScore) {
        bestTreeString = tree;
        bestScore = candidate.score;
    }
	if (treeTopologyExist(candidate.topology)) {
	    // if tree topology already exist, we replace the old
	    // by the new one (with new branch lengths) and update the score
		if (topologies[candidate.topology] < score) {
			topologies[candidate.topology] = score;
			for (CandidateSet::iterator i = begin(); i != end(); i++)
				if (i->second.topology == candidate.topology) {
					erase(i);
					break;
				}
			// insert tree into candidate set
			insert(CandidateSet::value_type(score, candidate));
		}
		newTree = false;
	} else {
		newTree = true;
		if (size() < maxCandidates) {
			// insert tree into candidate set
			insert(CandidateSet::value_type(score, candidate));
			topologies[candidate.topology] = score;
			candidateTreeVec.push_back(tree); // Diep added
		} else if (getWorstScore() <= score){
			// remove the worst-scoring tree
			topologies.erase(begin()->second.topology);

			// Diep added
			int n = candidateTreeVec.size();
			for(int i = 0; i < n; i++)
				if(candidateTreeVec[i] == begin()->second.tree){
					candidateTreeVec[i] = tree;
					break;
				}

			erase(begin());
			// insert tree into candidate set
			insert(CandidateSet::value_type(score, candidate));
			topologies[candidate.topology] = score;
		}else
			newTree = false; // Diep added
	}
	return newTree;
}

vector<double> CandidateSet::getBestScores(int numBestScore) {
	if (numBestScore == 0)
		numBestScore = size();
	vector<double> res;
	for (reverse_iterator rit = rbegin(); rit != rend() && numBestScore > 0; rit++, numBestScore--) {
		res.push_back(rit->first);
	}
	return res;
}

double CandidateSet::getWorstScore() {
	return begin()->first;
}

string CandidateSet::getTopology(string tree) {
	PhyloTree mtree;
	mtree.rooted = false;
	mtree.aln = aln;
	mtree.readTreeString(tree);
    mtree.root = mtree.findNodeName(aln->getSeqName(0));
	ostringstream ostr;
	mtree.printTree(ostr, WT_TAXON_ID | WT_SORT_TAXA);
	return ostr.str();
}

void CandidateSet::clear() {
	multimap<double, CandidateTree>::clear();
	topologies.clear();
	bestScore = -DBL_MAX;
}

CandidateSet CandidateSet::getBestCandidateTrees(int numTrees) {
	CandidateSet res;
	if (numTrees >= size())
		numTrees = size();
	for (reverse_iterator rit = rbegin(); rit != rend() && numTrees > 0; rit++, numTrees--) {
		res.insert(*rit);
	}
	return res;
}

CandidateSet::~CandidateSet() {
}

bool CandidateSet::treeTopologyExist(string topo) {
	return topologies.find(topo) != topologies.end();
}

bool CandidateSet::treeExist(string tree) {
	return treeTopologyExist(getTopology(tree));
}

void CandidateSet::copyToCandidateVec(){
	assert(!empty());
	if (empty())
		return;
	candidateTreeVec.clear();
	for (reverse_iterator i = rbegin(); i != rend(); i++)
		candidateTreeVec.push_back(i->second.tree);
}

string CandidateSet::getRandCandVecTree(){
	assert(!empty());
	if (empty())
		return "";
	int n = candidateTreeVec.size();
	int i = random_int(n);
	return candidateTreeVec[i];
	assert(0);
	return "";
}
