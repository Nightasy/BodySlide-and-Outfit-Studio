/*
BodySlide and Outfit Studio
Copyright (C) 2015  Caliente & ousnius
See the included LICENSE file
*/

#include "stdafx.h"
#include "Anim.h"

bool AnimInfo::AddShapeBone(const string& shape, AnimBone& boneDataRef) {
	for (auto &bone : shapeBones[shape])
		if (!bone.compare(boneDataRef.boneName))
			return false;

	shapeSkinning[shape].boneNames[boneDataRef.boneName] = shapeBones.size();
	shapeBones[shape].push_back(boneDataRef.boneName);
	AnimSkeleton::getInstance().RefBone(boneDataRef.boneName);
	return true;
}

bool AnimInfo::RemoveShapeBone(const string& shape, const string& boneName) {
	int bidx = 0;
	bool found = false;
	for (auto &bone : shapeBones[shape]) {
		if (bone.compare(boneName) == 0) {
			found = true;
			break;
		}
		bidx++;
	}

	if (!found)
		return false;

	shapeBones[shape].erase(shapeBones[shape].begin() + bidx);
	shapeSkinning[shape].RemoveBone(bidx);

	AnimSkeleton::getInstance().ReleaseBone(boneName);
	return true;
}

void AnimInfo::Clear() {
	if (refNif && refNif->IsValid()) {
		vector<string> shapes;
		refNif->GetShapeList(shapes);

		for (auto &shapeBoneList : shapeBones)
			for (auto &boneName : shapeBoneList.second)
				AnimSkeleton::getInstance().ReleaseBone(boneName);

		shapeSkinning.clear();
		for (auto &s : shapes)
			shapeBones[s].clear();

		refNif = nullptr;
	}
}

void AnimInfo::ClearShape(const string& shape) {
	for (auto &boneName : shapeBones[shape])
		AnimSkeleton::getInstance().ReleaseBone(boneName);

	shapeBones.erase(shape);
	shapeSkinning.erase(shape);
}

bool AnimInfo::LoadFromNif(NifFile* nif) {
	vector<string> shapes;
	nif->GetShapeList(shapes);

	Clear();

	for (auto &s : shapes)
		LoadFromNif(nif, s);

	refNif = nif;
	return true;
}

bool AnimInfo::LoadFromNif(NifFile* nif, const string& shape, bool newRefNif) {
	vector<string> boneNames;
	vector<int> BoneIndices;
	string invalidBones = "";

	if (newRefNif)
		refNif = nif;

	if (!nif->GetShapeBoneList(shape, boneNames)) {
		wxLogWarning("No skinning found in shape '%s' of '%s'.", shape, nif->GetFileName());
		return false;
	}

	int slot = 0;
	for (auto &bn : boneNames) {
		if (!AnimSkeleton::getInstance().RefBone(bn)) {
			AnimBone& cstm = AnimSkeleton::getInstance().AddBone(bn, true);
			if (!cstm.isValidBone)
				invalidBones += bn + "\n";

			vector<Vector3> r;
			nif->GetNodeTransform(bn, r, cstm.trans, cstm.scale);
			cstm.rot.Set(r);
			cstm.localRot = cstm.rot;
			cstm.localTrans = cstm.trans;

			AnimSkeleton::getInstance().RefBone(bn);
		}
		AnimBone* bonePtr = AnimSkeleton::getInstance().GetBonePtr(bn);
		if (bonePtr && !bonePtr->hasSkinXform) {
			SkinTransform shapeskinxform;
			Vector3 offs;
			float rad;
			if (nif->GetShapeBoneTransform(shape, bn, shapeskinxform, offs, rad)) {
				bonePtr->skinRot.Set(shapeskinxform.rotation);
				bonePtr->skinTrans = shapeskinxform.translation;
				bonePtr->hasSkinXform = true;
			}
		}
		shapeBones[shape].push_back(bn);
		BoneIndices.push_back(slot++);
	}

	shapeSkinning[shape] = AnimSkin(nif, shape, BoneIndices);

	if (!invalidBones.empty()) {
		wxLogWarning("Bones in shape '%s' not found in reference skeleton:\n%s", shape, invalidBones);
		wxMessageBox(wxString::Format("Bones in shape '%s' not found in reference skeleton:\n\n%s", shape, invalidBones), "Invalid Bones");
	}

	return true;
}

void AnimInfo::GetBoneXForm(const string& boneName, SkinTransform& stransform) {
	AnimBone b;
	if (AnimSkeleton::getInstance().GetBone(boneName, b)) {
		stransform.translation = b.localTrans;
		stransform.scale = b.scale;
		b.rot.GetRow(0, stransform.rotation[0]); // = b.rot[0];
		b.rot.GetRow(1, stransform.rotation[1]); // = b.rot[1];
		b.rot.GetRow(2, stransform.rotation[2]); // = b.rot[2];
	}
}

int AnimInfo::GetShapeBoneIndex(const string& shapeName, const string& boneName) {
	int b = -1;
	for (int i = 0; i < shapeBones[shapeName].size(); i++) {
		if (shapeBones[shapeName][i] == boneName) {
			b = i;
			break;
		}
	}

	return b;
}

void AnimInfo::GetWeights(const string& shape, const string& boneName, unordered_map<ushort, float>& outVertWeights) {
	int b = GetShapeBoneIndex(shape, boneName);
	if (b < 0)
		return;

	outVertWeights = shapeSkinning[shape].boneWeights[b].weights;
}

void AnimInfo::SetShapeBoneXForm(const string& shape, const string& boneName, SkinTransform& stransform) {
	int b = GetShapeBoneIndex(shape, boneName);
	if (b < 0)
		return;

	shapeSkinning[shape].boneWeights[b].xform = stransform;

}

bool AnimInfo::CalcShapeSkinBounds(const string& shape) {
	if (!refNif || !refNif->IsValid()) {						// check for existence of reference nif
		return false;
	}
	if (shapeSkinning.find(shape) == shapeSkinning.end()) {		// Check for shape in skinning data
		return false;
	}
	vector<Vector3> verts;
	refNif->GetVertsForShape(shape, verts);
	if (verts.size() == 0)										// check for empty shape
		return false;

	for (auto bn : shapeSkinning[shape].boneNames) {
		Vector3 a(FLT_MAX, FLT_MAX, FLT_MAX);
		Vector3 b(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (auto &w : shapeSkinning[shape].boneWeights[bn.second].weights) {
			if (w.first > verts.size()) {		// incoming weights have a larger set of possible verts. 
				return false;
			}
			Vector3 v = verts[w.first];
			a.x = min(a.x, v.x);
			a.y = min(a.y, v.y);
			a.z = min(a.z, v.z);
			b.x = max(b.x, v.x);
			b.y = max(b.y, v.y);
			b.z = max(b.z, v.z);
		}

		Vector3 tot = (a + b) / 2.0f;
		float d = 0.0f;
		for (auto &w : shapeSkinning[shape].boneWeights[bn.second].weights) {
			Vector3 v = verts[w.first];
			d = max(d, tot.DistanceTo(v));
		}

	   Matrix4 mat;
	   Vector3 trans;

	   AnimBone* xformRef = 	AnimSkeleton::getInstance().GetBonePtr(bn.first);
	   if (xformRef->hasSkinXform) {
		   mat = xformRef->skinRot;
		   trans = xformRef->skinTrans;
	   }
	   else {
			// Just FYI, I have no idea if the transform here is even slightly appropriate.  
		   mat = shapeSkinning[shape].boneWeights[bn.second].xform.ToMatrix();		   
	   }

		tot = mat * tot;
		tot = tot + trans;
		shapeSkinning[shape].boneWeights[bn.second].bSphereOffset = tot;
		shapeSkinning[shape].boneWeights[bn.second].bSphereRadius = d;

	}
	return true;
}

void AnimInfo::SetWeights(const string& shape, const string& boneName, unordered_map<ushort, float>& inVertWeights) {
	int bid = GetShapeBoneIndex(shape, boneName);
	if (bid == -1)
		return;

	shapeSkinning[shape].boneWeights[bid].weights.clear();
	shapeSkinning[shape].boneWeights[bid].weights = inVertWeights;

	if (refNif && refNif->IsValid()) {
		vector<Vector3> verts;
		refNif->GetVertsForShape(shape, verts);

		if (verts.size() == 0)	// early out for when the reference nif doesn't contain a matching shape for the incoming weights. 
			return;


		Vector3 a(FLT_MAX, FLT_MAX, FLT_MAX);
		Vector3 b(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (auto &w : inVertWeights) {
			if (w.first > verts.size()) {		// incoming weights have a larger set of possible verts. 
				return;
			}
			Vector3 v = verts[w.first];
			a.x = min(a.x, v.x);
			a.y = min(a.y, v.y);
			a.z = min(a.z, v.z);
			b.x = max(b.x, v.x);
			b.y = max(b.y, v.y);
			b.z = max(b.z, v.z);
		}

		Vector3 tot = (a + b) / 2.0f;
		float d = 0.0f;
		for (auto &w : inVertWeights) {
			Vector3 v = verts[w.first];
			d = max(d, tot.DistanceTo(v));
		}

		Matrix4 mat(shapeSkinning[shape].boneWeights[bid].xform.ToMatrix());
		tot = mat * tot;
		shapeSkinning[shape].boneWeights[bid].bSphereOffset = tot;
		shapeSkinning[shape].boneWeights[bid].bSphereRadius = d;
	}
	// Got all the way here, bounds calculation has been done.
	shapeSkinning[shape].bNeedsBoundsCalc = false;
}

void AnimInfo::WriteToNif(NifFile* nif, bool synchBoneIDs, const string& shapeException) {
	if (synchBoneIDs) {
		for (auto &bones : shapeBones) {
			if (bones.first == shapeException)
				continue;

			vector<int> bids;
			for (auto &bone : bones.second) {
				int id = nif->GetNodeID(bone);
				if (id == -1) {
					AnimBone boneRef;
					if (!AnimSkeleton::getInstance().GetBone(bone, boneRef))
						continue;
					if (boneRef.refCount == 0)
						continue;

					vector<Vector3> r(3);
					boneRef.rot.GetRow(0, r[0]);
					boneRef.rot.GetRow(1, r[1]);
					boneRef.rot.GetRow(2, r[2]);
					boneRef.scale = 1.0f;				// Bone scaling is bad!
					id = nif->AddNode(boneRef.boneName, r, boneRef.trans, boneRef.scale);
				}
				bids.push_back(id);
			}
			nif->SetShapeBoneIDList(bones.first, bids);
		}
	}

	SkinTransform xForm;
	for (auto &shapeBoneList : shapeBones) {
		if (shapeBoneList.first == shapeException)
			continue;
		int stype = nif->GetShapeType(shapeBoneList.first);
		bool bIsFo4 = (stype == BSTRISHAPE || stype == BSSUBINDEXTRISHAPE);
		
		if (shapeSkinning[shapeBoneList.first].bNeedsBoundsCalc) {
			CalcShapeSkinBounds(shapeBoneList.first);
		}

		unordered_map<unsigned short, vertexBoneWeights> vertWeights;
		for (auto &boneName : shapeBoneList.second) {
			if (!AnimSkeleton::getInstance().GetBoneTransform(boneName, xForm))
				continue;

			nif->SetNodeTransform(boneName, xForm);

			int bid = GetShapeBoneIndex(shapeBoneList.first, boneName);
			AnimWeight& bw = shapeSkinning[shapeBoneList.first].boneWeights[bid];
			if (bIsFo4) {
				for (auto vw : bw.weights) {
					vertWeights[vw.first].Add(bid, vw.second);
				}
				AnimBone* bptr = AnimSkeleton::getInstance().GetBonePtr(boneName);
				if (!bptr->hasSkinXform) {
					wxMessageBox("Warning: Bone information incomplete, exported data will not contain correct BSBoneData entries!  Be sure to Load a reference nif prior to export.", "Export warning");
					nif->SetShapeBoneTransform(shapeBoneList.first, bid, bw.xform, bw.bSphereOffset, bw.bSphereRadius);
				}
				else {
					SkinTransform st;
					st.rotation[0] = Vector3(bptr->skinRot[0], bptr->skinRot[1], bptr->skinRot[2]);
					st.rotation[1] = Vector3(bptr->skinRot[4], bptr->skinRot[5], bptr->skinRot[6]);
					st.rotation[2] = Vector3(bptr->skinRot[8], bptr->skinRot[9], bptr->skinRot[10]);
					st.translation = bptr->skinTrans;
					nif->SetShapeBoneTransform(shapeBoneList.first, bid,st, bw.bSphereOffset, bw.bSphereRadius);				
				}
			}
			else {
				if (AnimSkeleton::getInstance().GetSkinTransform(boneName, xForm)) {
					nif->SetShapeBoneTransform(shapeBoneList.first, bid, xForm, bw.bSphereOffset, bw.bSphereRadius);
					nif->SetShapeBoneWeights(shapeBoneList.first, bid, bw.weights);
				}
			}
		}
		if (bIsFo4) {
			for (auto vid : vertWeights) {
				nif->SetShapeVertWeights(shapeBoneList.first, vid.first, vid.second.boneIds, vid.second.weights);
			}
			
		}
	}
}

void AnimInfo::RenameShape(const string& shapeName, const string& newShapeName) {
	if (shapeSkinning.find(shapeName) != shapeSkinning.end()) {
		shapeSkinning[newShapeName] = move(shapeSkinning[shapeName]);
		shapeSkinning.erase(shapeName);
	}

	if (shapeBones.find(shapeName) != shapeBones.end()) {
		shapeBones[newShapeName] = move(shapeBones[shapeName]);
		shapeBones.erase(shapeName);
	}
}

AnimBone& AnimBone::LoadFromNif(NifFile* skeletonNif, int srcBlock, AnimBone* inParent)  {
	parent = inParent;
	isValidBone = false;
	NiNode* node = dynamic_cast<NiNode*>(skeletonNif->GetBlock(srcBlock));
	if (node)
		isValidBone = true;
	else
		return (*this);

	boneID = srcBlock;
	boneName = node->name;
	order = -1;
	refCount = 0;

	localRot.Set(node->rotation);
	localTrans = node->translation;
	scale = node->scale;

	if (parent) {
		trans = parent->trans + (parent->rot * localTrans);
		rot = parent->rot * localRot;
	}
	else {
		trans = localTrans;
		rot = localRot;
	}

	for (auto &c : node->children) {
		string name = skeletonNif->NodeName(c);
		if (!name.empty()){
			if (name == "_unnamed_")
				name = AnimSkeleton::getInstance().GenerateBoneName();

			AnimBone& bone = AnimSkeleton::getInstance().AddBone(name).LoadFromNif(skeletonNif, c, this);
			children.push_back(&bone);
		}
	}
	return (*this);
}

int AnimSkeleton::LoadFromNif(const string& fileName) {
	int error = refSkeletonNif.Load(fileName);
	if (error) {
		wxLogError("Failed to load skeleton '%s'!", fileName);
		wxMessageBox(wxString::Format("Failed to load skeleton '%s'!", fileName));
		return 1;
	}

	rootBone = Config.GetCString("Anim/SkeletonRootName");
	int nodeID = refSkeletonNif.GetNodeID(rootBone);
	if (nodeID == -1) {
		wxLogError("Root '%s' not found in skeleton '%s'!", rootBone, fileName);
		wxMessageBox(wxString::Format("Root '%s' not found in skeleton '%s'!", rootBone, fileName));
		return 2;
	}

	if (isValid)
		allBones.clear();

	AddBone(rootBone).LoadFromNif(&refSkeletonNif, nodeID, nullptr);
	isValid = true;
	wxLogMessage("Loaded skeleton '%s' with root '%s'.", fileName, rootBone);
	return 0;
}

AnimBone& AnimSkeleton::AddBone(const string& boneName, bool bCustom) {
	if (!bCustom)
		return allBones[boneName];
	else if (allowCustom)
		return customBones[boneName];
	else
		return invBone;
}

string AnimSkeleton::GenerateBoneName() {
	return wxString::Format("UnnamedBone_%i", unknownCount++).ToStdString();
}
	
bool AnimSkeleton::RefBone(const string& boneName) {
	if (allBones.find(boneName) != allBones.end()) {
		allBones[boneName].refCount++;
		return true;
	}
	if (allowCustom && customBones.find(boneName) != customBones.end()) {
		customBones[boneName].refCount++;
		return true;
	}
	return false;
}
	
bool AnimSkeleton::ReleaseBone(const string& boneName) {
	if (allBones.find(boneName) != allBones.end()) {
		allBones[boneName].refCount--;
		return true;
	}
	if (allowCustom && customBones.find(boneName) != customBones.end()) {
		customBones[boneName].refCount--;
		return true;
	}
	return false;
}

AnimBone* AnimSkeleton::GetBonePtr(const string& boneName) {
	if (boneName.empty())
		return &allBones[rootBone];

	if (allBones.find(boneName) != allBones.end())
		return &allBones[boneName];

	if (allowCustom && customBones.find(boneName) != customBones.end())
		return &customBones[boneName];

	return nullptr;
}

bool AnimSkeleton::GetBone(const string& boneName, AnimBone& outBone) {
	if (allBones.find(boneName) != allBones.end()) {
		outBone = allBones[boneName];
		return true;
	}
	if (allowCustom && customBones.find(boneName) != customBones.end()) {
		outBone = customBones[boneName];
		return true;
	}
	return false;
}

bool AnimSkeleton::GetBoneTransform(const string &boneName, SkinTransform& xform) {
	if (allBones.find(boneName) == allBones.end())
		return false;

	AnimBone* cB = &allBones[boneName];
	Matrix4 rot = cB->rot;
	rot.GetRow(0, xform.rotation[0]);
	rot.GetRow(1, xform.rotation[1]);
	rot.GetRow(2, xform.rotation[2]);
	xform.scale = 1.0f; //cB->scale					// Scale should be ignored?
	xform.translation = cB->trans;
	//xform.translation = cB->trans * -1.0f;
	//xform.translation = rot * xform.translation;
	return true;
}

bool AnimSkeleton::GetSkinTransform(const string &boneName, SkinTransform& xform) {
	if (allBones.find(boneName) == allBones.end())
		return false;

	AnimBone* cB = &allBones[boneName];
	Matrix4 rot = cB->rot.Inverse();
	rot.GetRow(0, xform.rotation[0]);
	rot.GetRow(1, xform.rotation[1]);
	rot.GetRow(2, xform.rotation[2]);
	xform.scale = 1.0f;	//cB->scale					// Scale should be ignored?
	xform.translation = cB->trans * -1.0f;
	xform.translation = rot * xform.translation;
	return true;
}

int AnimSkeleton::GetActiveBoneNames(vector<string>& outBoneNames) {
	int c = 0;
	for (auto &ab : allBones) {
		if (ab.second.refCount > 0) {
			outBoneNames.push_back(ab.first);
			c++;
		}
	}
	return c;
}
