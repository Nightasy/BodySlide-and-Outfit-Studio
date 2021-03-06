/*
BodySlide and Outfit Studio
Copyright (C) 2015  Caliente & ousnius
See the included LICENSE file
*/

#pragma once

#include "NifFile.h"
#include "ObjFile.h"
#include "DiffData.h"
#include "SliderData.h"
#include "Automorph.h"
#include "Mesh.h"
#include "OutfitStudio.h"
#include "ConfigurationManager.h"
#include "Anim.h"

#include <map>
#include <vector>


#pragma warning (disable: 4018)
using namespace std;

class OutfitStudio;

class OutfitProject {
	string defaultTexFile = "res\\NoImg.png";
	ConfigurationManager& appConfig;
	OutfitStudio* owner;

	NifFile workNif;
	AnimInfo workAnim;
	string baseShape;

public:
	string outfitName;
	DiffDataSets baseDiffData;
	SliderSet activeSet;
	Automorph morpher;
	bool morpherInitialized;

	map<string, vector<Vector3>> boneScaleOffsets;
	map<string, vector<Vector3>> boneScaleVerts;
	map<string, unordered_map<ushort, float>> workWeights;

	map<string, bool> shapeDirty;
	unordered_map<string, string> shapeTextures;

	// inOwner is meant to provide access to OutfitStudio for the purposes of reporting process status only.
	OutfitProject(ConfigurationManager& inConfig, OutfitStudio* inOwner = nullptr);
	~OutfitProject();
	string mFileName;
	string mOutfitName;
	string mDataDir;
	string mBaseFile;
	string mGamePath;
	string mGameFile;
	bool mCopyRef;
	bool mGenWeights;

	// Returns a string error message or empty string on success.
	string Save(const string& strFileName,
		const string& strOutfitName,
		const string& strDataDir,
		const string& strBaseFile,
		const string& strGamePath,
		const string& strGameFile,
		bool genWeights,
		bool copyRef);

	NifFile* GetWorkNif() { return &workNif; }
	AnimInfo* GetWorkAnim() { return &workAnim; }
	string GetBaseShape() { return baseShape; }
	void SetBaseShape(const string& shapeName) { baseShape = shapeName; }

	bool IsBaseShape(const string& shapeName) {
		return shapeName == baseShape;
	}

	string SliderSetName();
	string SliderSetFileName();
	string OutfitName();

	bool IsDirty();
	void Clean() {
		shapeDirty.clear();
	}
	void Clean(const string& specificShape);
	void SetDirty(const string& specificShape);
	bool IsDirty(const string& specificShape);

	bool ValidSlider(int index);
	bool ValidSlider(const string& sliderName);
	bool AllSlidersZero();
	int SliderCount(void);
	string GetSliderName(int index);
	void GetSliderList(vector<string>& sliderNames);
	void AddEmptySlider(const string& newName);
	void AddZapSlider(const string& newName, unordered_map<ushort, float>& verts, const string& shapeName);
	void AddCombinedSlider(const string& newName);

	// AddShapeFromObjFile - shapeName is modified during a successful import to reflect the name of the news hape.
	int AddShapeFromObjFile(const string& fileName, string& shapeName, const string& mergeShape = ""); 

	int CreateNifShapeFromData(const string& shapeName,  vector<Vector3>& v,  vector<Triangle>& t,  vector<Vector2>& uv, vector<Vector3>* norms = nullptr);

	// Slider data can have a separate name from the shape target.
	string SliderShapeDataName(int index, const string& shapeName);
	bool SliderClamp(int index);
	bool SliderZap(int index);
	bool SliderInvert(int index);
	bool SliderHidden(int index);
	int SliderIndexFromName(const string& sliderName);

	void SetSliderZap(int index, bool zap);
	void SetSliderInvert(int index, bool inv);
	void SetSliderHidden(int index, bool hidden);
	void SetSliderDefault(int index, int val, bool isHi);
	void SetSliderName(int index, const string& newName);

	void NegateSlider(const string& sliderName, const string& shapeName);

	void SetSliderFromBSD(const string& sliderName, const string& shapeName, const string& fileName);
	bool SetSliderFromOBJ(const string& sliderName, const string& shapeName, const string& fileName);
	bool SetSliderFromFBX(const string& sliderName, const string& shapeName, const string& fileName);
	void SetSliderFromTRI(const string& sliderName, const string& shapeName, unordered_map<ushort, Vector3>& diff);
	int SaveSliderBSD(const string& sliderName, const string& shapeName, const string& fileName);
	int SaveSliderOBJ(const string& sliderName, const string& shapeName, const string& fileName);
	int WriteMorphTRI(const string& triPath);

	float& SliderValue(int index);
	float& SliderValue(const string& name);
	float SliderDefault(int index, bool hi);

	void InitConform();
	void ConformShape(const string& shapeName);

	const string& ShapeToTarget(const string& shapeName);
	int GetVertexCount(const string& shapeName);
	void GetLiveVerts(const string& shapeName, vector<Vector3>& outVerts);
	void GetShapes(vector<string>& outShapeNames);
	void GetActiveBones(vector<string>& outBoneNames);
	void GetBones(vector<string>& outBoneNames);

	string GetShapeTexture(const string& shapeName);

	void SetTexturesDefault(const string& defaultChoice);
	void SetTextures(const string& textureFile);
	void SetTexture(const string& shapeName, const string& textureFile);

	bool IsValidShape(const string& shapeName);

	bool& SliderShow(int index);
	bool& SliderShow(const string& sliderName);

	void RefreshMorphShape(const string& shapeName);
	void UpdateShapeFromMesh(const string& shapeName, const mesh* m);
	void UpdateMorphResult(const string& shapeName, const string& sliderName, unordered_map<ushort, Vector3>& vertUpdates);
	void ScaleMorphResult(const string& shapeName, const string& sliderName, float scaleValue);
	void MoveVertex(const string& shapeName, const Vector3& pos, const int& id);
	void OffsetShape(const string& shapeName, const Vector3& xlate, unordered_map<ushort, float>* mask = nullptr);
	void ScaleShape(const string& shapeName, const float& scale, unordered_map<ushort, float>* mask = nullptr);
	void RotateShape(const string& shapeName, const Vector3& angle, unordered_map<ushort, float>* mask = nullptr);

	void AutoOffset(NifFile& nif);

	// Uses the AutoMorph class to generate proximity values for bone weights.
	// This is done by creating several virtual sliders that contain weight offsets for each vertex per bone.
	// These data sets are then temporarily linked to the AutoMorph class and result 'diffs' are generated.
	// The resulting data is then written back to the outfit shape as the green color channel.
	void CopyBoneWeights(const string& destShape, unordered_map<ushort, float>* mask = nullptr, vector<string>* inBoneList = nullptr);
	// Transfers the weights of the selected bones from reference to chosen shape 1:1. Requires same vertex count and order.
	void TransferSelectedWeights(const string& destShape, unordered_map<ushort, float>* mask = nullptr, vector<string>* inBoneList = nullptr);
	bool HasUnweighted();

	void ApplyBoneScale(const string& bone, int sliderPos, bool clear = false);
	void ClearBoneScale(bool clear = true);

	void AddBoneRef(const string& boneName);

	// Rebuilds skin partitions in the nif.  Games use the skin partition (As opposed to the skindata) for animation,
	// so fresh meshes need to have the partitions created. Note, when updating bone weights, rebuilding the skin partitions is not
	// typically required, and bone weight assignment is taken care of in the NIF save operations.
	void BuildShapeSkinPartions(const string& destShape);

	void ClearWorkSliders();
	void ClearReference();
	void ClearOutfit();
	void ClearSlider(const string& shapeName, const string& sliderName);
	void ClearUnmaskedDiff(const string& shapeName, const string& sliderName, unordered_map<ushort, float>* mask);
	void DeleteSlider(const string& sliderName);

	int LoadSkeletonReference(const string& skeletonFileName);
	int LoadReferenceTemplate(const string& templateName, bool clearRef = true);
	int LoadReferenceNif(const string& fileName, const string& shapeName, bool ClearRef = true);
	int LoadReference(const string& fileName, const string& setName, bool ClearRef = true, const string& shapeName = "");
	int AddNif(const string& fileName, bool clear = true, const string& inOutfitName = "");

	int OutfitFromSliderSet(const string& fileName, const string& setName);

	/* Shape duplication - resulting shape ends up in workNif. */
	void DuplicateShape(const string& sourceShape, const string& destShape, const mesh* curMesh);

	void DeleteShape(const string& shapeName) {
		workAnim.ClearShape(shapeName);
		workNif.DeleteShape(shapeName);
	}

	void DeleteBone(const string& boneName) {
		vector<string> shapes;
		if (workNif.IsValid()) {
			GetShapes(shapes);
			for (auto &s : shapes)
				workAnim.RemoveShapeBone(s, boneName);

			int blockID = workNif.GetNodeID(boneName);
			if (blockID >= 0)
				workNif.DeleteBlock(blockID);
		}
	}


	void RenameShape(const string& shapeName, const string& newShapeName);

	void UpdateNifNormals(NifFile* nif, const vector<mesh*>& shapemeshes);
	int SaveOutfitNif(const string& fileName, const vector<mesh*>& modMeshes, bool writeNormals, bool withRef = false);

	int ImportShapeFBX(const string& fileName, const string& shapeName = "", const string& mergeShape = "");
	int ExportShapeFBX(const string& fileName, const string& shapeName = "");
	int ExportShapeObj(const string& fileName, const string& shapeName, Vector3 scale = Vector3(1.0f, 1.0f, 1.0f), Vector3 offset = Vector3());

	/* Creates an abbreviated name for use in data file identifiers. Mostly removes spaces and other special characters. */
	string NameAbbreviate(const string& inputName);
};
