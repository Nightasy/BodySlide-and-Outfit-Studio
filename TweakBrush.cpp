/*
BodySlide and Outfit Studio
Copyright (C) 2015  Caliente & ousnius
See the included LICENSE file
*/

#include "TweakBrush.h"
#pragma warning (disable : 4100)

TweakUndo::TweakUndo() : curState(-1) {
}

TweakUndo::~TweakUndo() {
	Clear();
}

void TweakUndo::Clear()	{
	for (unsigned int i = 0; i < strokes.size(); i++) {
		delete strokes[i];
	}
	strokes.clear();
	curState = -1;
}

int TweakStroke::nStrokes = 0;
int TweakStroke::outPositionCount = 0;
Vector3* TweakStroke::outPositions = nullptr;

TweakStroke* TweakUndo::CreateStroke(mesh* refmesh, TweakBrush* refBrush) {
	TweakStroke* newStroke = new TweakStroke(refmesh, refBrush);
	addStroke(newStroke);
	return newStroke;
}

void TweakUndo::addStroke(TweakStroke* stroke) {
	int maxState = strokes.size() - 1;
	if (curState != maxState) {
		for (auto strokeIt = strokes.begin() + (curState + 1); strokeIt != strokes.end(); ++strokeIt)
			delete (*strokeIt);

		strokes.erase(strokes.begin() + (curState + 1), strokes.end());
		curState++;
	}
	else if (strokes.size() == TB_MAX_UNDO) {
		delete strokes[0];
		strokes.erase(strokes.begin());
	}
	else {
		curState++;
	}
	strokes.push_back(stroke);
}

bool TweakUndo::backStroke(bool skipUpdate) {
	if (curState > -1) {
		if (!skipUpdate)
			strokes[curState]->restoreStartState();
		curState--;
		return true;
	}
	return false;
}

bool TweakUndo::forwardStroke(bool skipUpdate) {
	int maxState = strokes.size() - 1;
	if (curState < maxState) {
		curState++;
		if (!skipUpdate)
			strokes[curState]->restoreEndState();
		return true;
	}
	return false;
}

void TweakStroke::restoreStartState() {
	for (auto &stateIt : pointStartState){
		if (refBrush->Type() == TBT_MASK || refBrush->Type() == TBT_WEIGHT) {
			refMesh->vcolors[stateIt.first] = stateIt.second;
		}
		else {
			refMesh->verts[stateIt.first] = stateIt.second;
		}
	}
	if (refBrush->Type() != TBT_MASK &&   refBrush->Type() != TBT_WEIGHT) {
		refMesh->SmoothNormals();
		if (startBVH == endBVH) {
			for (auto &bvhNode : affectedNodes) {
				bvhNode->UpdateAABB();
			}
		}
		else {
			refMesh->bvh = startBVH;
		}
	}
}

void TweakStroke::restoreEndState() {
	for (auto &stateIt : pointEndState) {
		if (refBrush->Type() == TBT_MASK || refBrush->Type() == TBT_WEIGHT) {
			refMesh->vcolors[stateIt.first] = stateIt.second;
		}
		else {
			refMesh->verts[stateIt.first] = stateIt.second;
		}
	}

	if (refBrush->Type() != TBT_MASK && refBrush->Type() != TBT_WEIGHT) {
		refMesh->SmoothNormals();
		if (startBVH == endBVH) {
			for (auto &bvhNode : affectedNodes) {
				bvhNode->UpdateAABB();
			}
		}
		else
			refMesh->bvh = endBVH;
	}
}

void TweakStroke::beginStroke(TweakPickInfo& pickInfo) {
	refBrush->strokeInit(refMesh, pickInfo);
	startBVH = refMesh->bvh;

	pts1 = (int*)malloc(refMesh->nVerts*sizeof(int));
	if (refBrush->isMirrored())
		pts2 = (int*)malloc(refMesh->nVerts*sizeof(int));
}

void TweakStroke::updateStroke(TweakPickInfo& pickInfo) {
	vector<int> facets;
	vector<int> facets2;
	unordered_map<int, Vector3> movedpoints;

	int nPts1 = 0;
	int nPts2 = 0;

	int brushType = refBrush->Type();

	TweakPickInfo mirrorPick = pickInfo;
	mirrorPick.origin.x *= -1;
	mirrorPick.normal.x *= -1;
	mirrorPick.facet = pickInfo.facetM;

	if (!newStroke) {
		if (!refBrush->checkSpacing(lastPoint, pickInfo.origin))
			return;
	}
	else {
		newStroke = false;
	}


	if (brushType == TBT_MOVE || brushType == TBT_XFORM) {
		// Move brush handles most operations differently than other brushes.  Mirroring, for instance, is done internally. 
		// most of the pick info values are ignored.
		if (!refBrush->queryPoints(refMesh, pickInfo, nullptr, nPts1, facets, affectedNodes))
			return;

		if (nPts1 > outPositionCount) {
			if (outPositions)
				delete[] outPositions;

			outPositions = new Vector3[nPts1];
			outPositionCount = nPts1;
		}

		refBrush->brushAction(refMesh, pickInfo, nullptr, nPts1, outPositions);

		for (int i = 0; i < nPts1; i++)
			addPoint(refBrush->CachedPointIndex(i), outPositions[i]);
	}
	else {
		if (!refBrush->queryPoints(refMesh, pickInfo, pts1, nPts1, facets, affectedNodes))
			return;

		if (refBrush->isMirrored())
			refBrush->queryPoints(refMesh, mirrorPick, pts2, nPts2, facets2, affectedNodes);

		int totalpoints = max(nPts1, nPts2);
		if (totalpoints > outPositionCount) {
			if (outPositions)
				delete[] outPositions;

			outPositions = new Vector3[totalpoints];
			outPositionCount = totalpoints;
		}

		refBrush->brushAction(refMesh, pickInfo, pts1, nPts1, outPositions);
		for (int i = 0; i < nPts1; i++)
			addPoint(pts1[i], outPositions[i]);

		if (refBrush->isMirrored())  {
			refBrush->brushAction(refMesh, mirrorPick, pts2, nPts2, outPositions);
			for (int i = 0; i < nPts2; i++)
				addPoint(pts2[i], outPositions[i]);
		}
	}

	lastPoint = pickInfo.origin;

	if (refBrush->LiveNormals() && brushType != TBT_WEIGHT)
		refMesh->SmoothNormals();

	if (refBrush->LiveBVH() && brushType != TBT_WEIGHT) {
		for (auto &bvhNode : affectedNodes)
			bvhNode->UpdateAABB();
	}
}

void TweakStroke::endStroke() {
	if (refBrush->Type() == TBT_MOVE) {
		TB_Move* br = dynamic_cast<TB_Move*>(refBrush);
		if (br) {
			affectedNodes.swap(((TB_Move*)refBrush)->cachedNodes);
			affectedNodes.insert(((TB_Move*)refBrush)->cachedNodesM.begin(), ((TB_Move*)refBrush)->cachedNodesM.end());
			((TB_Move*)refBrush)->cachedNodes.clear();
			((TB_Move*)refBrush)->cachedNodesM.clear();
		}
	}
	else if (refBrush->Type() == TBT_XFORM)
		refMesh->CreateBVH();

	if (refBrush->Type() != TBT_WEIGHT)
		for (auto &bvhNode : affectedNodes)
			bvhNode->UpdateAABB();

	if (!refBrush->LiveNormals() || refBrush->Type() == TBT_WEIGHT)
		refMesh->SmoothNormals();

	if (pts1) free(pts1);
	if (pts2) free(pts2);

	endBVH = refMesh->bvh;
}

void TweakStroke::addPoint(int point, Vector3& newPos) {
	if (pointStartState.find(point) == pointStartState.end())
		pointStartState[point] = newPos;

	if (refBrush->Type() == TBT_MASK || refBrush->Type() == TBT_WEIGHT)
		pointEndState[point] = refMesh->vcolors[point];
	else
		pointEndState[point] = refMesh->verts[point];
}

TweakBrush::TweakBrush(void) : radius(0.45f), focus(1.00f), inset(0.00f), strength(0.0015f), spacing(0.015f) {
	brushType = TBT_STANDARD;
	brushName = "Standard Brush";
	bMirror = false;
	bLiveBVH = true;
	bLiveNormals = true;
	bConnected = true;
}

TweakBrush::~TweakBrush(void) {
}

bool TweakBrush::checkSpacing(Vector3& start, Vector3& end) {
	float d = start.DistanceTo(end);
	if (d > spacing)
		return true;
	else
		return false;
}

// Standard falloff function, used by most brushes 
//   y = (  (cos((pi/2)*x) * sqrt(cos((pi/2)*x))) ^ focus) - inset
//  cancel that -- going for a simpler function (1-x^2)*(1-x^4)
// with focus :  (1-(((f-1)-f*x)^2))*(1-(((f-1)-f*x)^4))  f is greater than 1.
//				
// values between 0 and 1 give a spherical curve, values over 1 give a peaked curve
void TweakBrush::applyFalloff(Vector3 &deltaVec, float dist) {
	// beyond the radius, always 0 stength.
	if (dist > radius) {
		deltaVec.x = deltaVec.y = deltaVec.z = 0.0f;
		return;
	}
	// at 0, always full strength
	if (dist == 0)
		return;

	float p = 1.0f;

	float x = dist / radius;
	if (x <= (focus - 1.0f) / (focus)) {
		p = 1.0f;
	}
	else {
		float fx;
		float p1 = 0.0f;
		float p2 = 0.0f;
		if (focus >= 1.0f) {
			fx = (focus - 1.0f) - (focus*x);
			p1 = (1.0f - fx*fx);
			p2 = (1.0f - pow(fx, 4));
		}
		else if (focus > 0.0f) {
			fx = x / focus;
			p1 = (1.0f - x*x);
			p2 = (1.0f - pow(fx, 4));
		}

		p = p1*p2;
		if (p < 0.0f) p = 0.0f;
	}
	deltaVec *= p;
}

bool TweakBrush::queryPoints(mesh *refmesh, TweakPickInfo& pickInfo, int* resultPoints, int& outResultCount, vector<int>& resultFacets, std::unordered_set<AABBTree::AABBTreeNode*> &affectedNodes) {
	vector<IntersectResult> IResults;

	if (!refmesh->bvh->IntersectSphere(pickInfo.origin, radius, &IResults))
		return false;

	bool* pointVisit = (bool*)calloc(refmesh->nVerts, sizeof(bool));

	if (bConnected) {
		bool* triVisit = (bool*)calloc(refmesh->nTris, sizeof(bool));
		refmesh->ConnectedPointsInSphere(pickInfo.origin, radius*radius, pickInfo.facet, triVisit, pointVisit, resultPoints, outResultCount, resultFacets);
		free(triVisit);
	}
	else {
		outResultCount = 0;
	}

	Triangle t;
	for (unsigned int i = 0; i < IResults.size(); i++) {
		if (!bConnected) {
			resultFacets.push_back(IResults[i].HitFacet);
			t = refmesh->tris[IResults[i].HitFacet];
			if (!pointVisit[t.p1]){
				resultPoints[outResultCount++] = t.p1;
				pointVisit[t.p1] = true;
			}
			if (!pointVisit[t.p2]){
				resultPoints[outResultCount++] = t.p2;
				pointVisit[t.p2] = true;
			}
			if (!pointVisit[t.p3]){
				resultPoints[outResultCount++] = t.p3;
				pointVisit[t.p3] = true;
			}

		}
		affectedNodes.insert(IResults[i].bvhNode);
	}

	free(pointVisit);
	return true;
}

void TweakBrush::brushAction(mesh *refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, unordered_map<int, Vector3> &movedpoints) {
	Matrix4 xform;
	xform.Translate(pickInfo.normal * strength);
	Vector3 vs;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i < nPoints; i++) {
		vs = refmesh->verts[points[i]];
		movedpoints[points[i]] = vs;
		ve = xform * vs;
		ve -= vs;
		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		if (refmesh->vcolors)
			ve = ve * (1.0f - refmesh->vcolors[points[i]].x);
		vf = vs + ve;
		refmesh->verts[points[i]] = (vf);
	}
}

void TweakBrush::brushAction(mesh *refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, Vector3* movedpoints) {
	Matrix4 xform;
	xform.Translate(pickInfo.normal * strength);
	Vector3 vs;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i < nPoints; i++) {
		vs = refmesh->verts[points[i]];
		movedpoints[i] = vs;
		ve = xform * vs;
		ve -= vs;
		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		if (refmesh->vcolors)
			ve = ve * (1.0f - refmesh->vcolors[points[i]].x);
		vf = vs + ve;
		refmesh->verts[points[i]] = (vf);
	}
}

TB_Mask::TB_Mask() :TweakBrush() {
	brushType = TBT_MASK;
	strength = 0.1f;
	focus = 5.0f;
	brushName = "Mask Brush";
}

TB_Mask::~TB_Mask() {
}

void TB_Mask::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, unordered_map<int, Vector3>& movedpoints) {
	Vector3 vs;
	Vector3 vc;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i<nPoints; i++) {

		vs = refmesh->verts[points[i]];
		vc = refmesh->vcolors[points[i]];
		movedpoints[points[i]] = vc;

		ve = vc;
		ve.x += strength;
		ve -= vc;

		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		vf = vc + ve;
		if (vf.x > 1.0f) vf.x = 1.0f;
		refmesh->vcolors[points[i]] = (vf);
	}
}
	
void TB_Mask::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, Vector3* movedpoints) {
	Vector3 vs;
	Vector3 vc;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i<nPoints; i++) {

		vs = refmesh->verts[points[i]];
		vc = refmesh->vcolors[points[i]];
		movedpoints[i] = vc;

		ve = vc;
		ve.x += strength;
		ve -= vc;

		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		vf = vc + ve;
		if (vf.x > 1.0f) vf.x = 1.0f;
		refmesh->vcolors[points[i]] = (vf);
	}
}

TB_Unmask::TB_Unmask() :TweakBrush() {
	brushType = TBT_MASK;
	strength = -0.1f;
	focus = 5.0f;
	brushName = "Unmask Brush";
}

TB_Unmask::~TB_Unmask() {
}

void TB_Unmask::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, unordered_map<int, Vector3>& movedpoints) {
	Vector3 vs;
	Vector3 vc;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i < nPoints; i++) {

		vs = refmesh->verts[points[i]];
		vc = refmesh->vcolors[points[i]];
		movedpoints[points[i]] = vc;

		ve = vc;
		ve.x += strength;
		ve -= vc;

		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		vf = vc + ve;
		if (vf.x < 0.0f) vf.x = 0.0f;
		refmesh->vcolors[points[i]] = (vf);
	}
}

void TB_Unmask::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, Vector3* movedpoints) {
	Vector3 vs;
	Vector3 vc;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i < nPoints; i++) {
		vs = refmesh->verts[points[i]];
		vc = refmesh->vcolors[points[i]];
		movedpoints[i] = vc;

		ve = vc;
		ve.x += strength;
		ve -= vc;

		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		vf = vc + ve;
		if (vf.x < 0.0f) vf.x = 0.0f;
		refmesh->vcolors[points[i]] = (vf);
	}
}
	
TB_Deflate::TB_Deflate() :TweakBrush() {
	strength = -0.0015f;
	brushName = "Deflate Brush";
}

TB_Deflate::~TB_Deflate() {
}

void TB_Deflate::setStrength(float newStr) {
	strength = -(newStr / 10.0f);
}

float TB_Deflate::getStrength() {
	return -strength * 10.0f;
}

TB_Smooth::TB_Smooth() :TweakBrush() {
	iterations = 1;
	method = 1;
	strength = 0.01f;
	hcAlpha = 0.2f;
	hcBeta = 0.5f;
	bMirror = false;
	b = nullptr;
	lastMesh = nullptr;
	brushName = "Smooth Brush";
}

TB_Smooth::~TB_Smooth() {
	if (b) {
		free(b);
		b = nullptr;
	}
}

void TB_Smooth::lapFilter(mesh* refmesh, int* points, int nPoints, unordered_map <int, Vector3>& wv) {
	unordered_map<int, Vector3>::iterator mi;
	Vector3 d;
	int adjPoints[1000];
	int c = 0;
	int a;

	for (int i = 0; i < nPoints; i++) {
		c = refmesh->GetAdjacentPoints(points[i], adjPoints, 1000);
		if (c == 0) continue;
		d.x = d.y = d.z = 0;
		// average adjacent points positions.  Since we're storing the changed vertices separately,
		// there's additional complexity involved with making sure we're grabbing a changed vertex 
		// instead of the original. This primarily comes into play when more than one iteration is used.
		for (int n = 0; n < c; n++) {
			a = adjPoints[n];
			mi = wv.find(a);
			if (mi != wv.end()) {
				d += mi->second;
			}
			else {
				d.x += refmesh->verts[a].x;
				d.y += refmesh->verts[a].y;
				d.z += refmesh->verts[a].z;
			}
		}
		wv[points[i]] = d / (float)c;

		if (refmesh->weldVerts.find(points[i]) != refmesh->weldVerts.end()) {
			for (unsigned int v = 0; v < refmesh->weldVerts[points[i]].size(); v++) {
				wv[refmesh->weldVerts[points[i]][v]] = wv[points[i]];
			}
		}
	}
}

void TB_Smooth::hclapFilter(mesh* refmesh, int* points, int nPoints, unordered_map <int, Vector3>& wv) {
	unordered_map<int, Vector3>::iterator mi;

	if (refmesh != lastMesh) {
		if (b) free(b);
		b = (Vector3*)calloc(refmesh->nVerts, sizeof(Vector3));
		lastMesh = refmesh;
	}
	else {
		memset(b, 0, refmesh->nVerts * sizeof(Vector3));
	}

	Vector3 d;
	Vector3 q;
	int adjPoints[1000];
	int a;
	int c;
	int i;
	// First step is to calculate the laplacian
	for (int p = 0; p < nPoints; p++) {
		i = points[p];
		c = refmesh->GetAdjacentPoints(i, adjPoints, 1000);
		if (c == 0) continue;
		d.x = d.y = d.z = 0;
		// average adjacent points positions.  Since we're storing the changed vertices separately,
		// there's additional complexity involved with making sure we're grabbing a changed vertex 
		// instead of the original. This primarily comes into play when more than one iteration is used.
		for (int n = 0; n < c; n++) {
			a = adjPoints[n];
			mi = wv.find(a);
			if (mi != wv.end()) {
				d += mi->second;
			}
			else {
				d.x += refmesh->verts[a].x;
				d.y += refmesh->verts[a].y;
				d.z += refmesh->verts[a].z;
			}
		}
		// Save the current point's working position (or original if the working value hasn't been calculated
		// yet.)  This is used as part of the blend between original and changed position
		mi = wv.find(i);
		if (mi != wv.end())
			q = wv[i];
		else
			q = refmesh->verts[i];

		wv[i] = d / (float)c;
		// Calculate the difference between the new position and a blend of the original and previous positions
		b[i] = wv[i] - ((refmesh->verts[i] * hcAlpha) + (q * (1.0f - hcAlpha)));

		if (refmesh->weldVerts.find(i) != refmesh->weldVerts.end()) {
			for (unsigned int v = 0; v < refmesh->weldVerts[i].size(); v++) {
				wv[refmesh->weldVerts[i][v]] = wv[i];
			}
		}
	}

	for (int p = 0; p < nPoints; p++) {
		int j = points[p];
		c = refmesh->GetAdjacentPoints(j, adjPoints, 1000);
		if (c == 0) continue;
		d.x = d.y = d.z = 0;
		for (int n = 0; n < c; n++)
			d += b[adjPoints[n]];

		// blend the new position and the average of the distance moved
		float avgB = (1 - hcBeta) / (float)c;
		wv[j] -= ((b[j] * hcBeta) + (d * avgB));

		if (refmesh->weldVerts.find(j) != refmesh->weldVerts.end())
			for (unsigned int v = 0; v < refmesh->weldVerts[j].size(); v++)
				wv[refmesh->weldVerts[j][v]] = wv[j];
	}
}

void TB_Smooth::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, unordered_map<int, Vector3>& movedpoints) {
	unordered_map<int, Vector3> wv;
	Vector3 vs;

	for (int i = 0; i < nPoints; i++) {
		movedpoints[points[i]] = Vector3(refmesh->verts[points[i]]);
		wv[points[i]] = movedpoints[points[i]];
	}


	for (int iter = 0; iter < iterations; iter++) {
		if (method == 0) {			// laplacian smooth
			lapFilter(refmesh, points, nPoints, wv);
		}
		else {					// HC-laplacian smooth
			hclapFilter(refmesh, points, nPoints, wv);
		}
	}
	Vector3 delta;
	if (strength != 1.0f) {
		for (int p = 0; p < nPoints; p++) {
			int i = points[p];
			vs = refmesh->verts[i];
			delta = wv[i] - refmesh->verts[i];
			delta *= strength;
			applyFalloff(delta, pickInfo.origin.DistanceTo(vs));

			if (refmesh->vcolors)
				delta = delta * (1.0f - refmesh->vcolors[i].x);
			refmesh->verts[i] += delta;
		}
	}
}

void TB_Smooth::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, Vector3* movedpoints) {
	unordered_map<int, Vector3> wv;
	Vector3 vs;

	for (int i = 0; i < nPoints; i++) {
		movedpoints[i] = refmesh->verts[points[i]];
		wv[points[i]] = movedpoints[i];
	}

	for (int iter = 0; iter < iterations; iter++) {
		if (method == 0) {			// laplacian smooth
			lapFilter(refmesh, points, nPoints, wv);
		}
		else {					// HC-laplacian smooth
			hclapFilter(refmesh, points, nPoints, wv);
		}
	}
	Vector3 delta;
	if (strength != 1.0f) {
		for (int p = 0; p < nPoints; p++) {
			int i = points[p];
			vs = refmesh->verts[i];
			delta = wv[i] - refmesh->verts[i];
			delta *= strength;
			applyFalloff(delta, pickInfo.origin.DistanceTo(vs));

			if (refmesh->vcolors)
				delta = delta * (1.0f - refmesh->vcolors[i].x);
			refmesh->verts[i] += delta;
		}
	}
}

TB_Move::TB_Move(void) : TweakBrush() {
	brushType = TBT_MOVE;
	brushName = "Move Brush";
	bLiveBVH = false;
	focus = 2.0f;
	strength = 0.1f;
	cachedPoints = nullptr;
	cachedPointsM = nullptr;
}

TB_Move::~TB_Move(void) {
	if (cachedPoints)
		free(cachedPoints);
	if (cachedPointsM)
		free(cachedPointsM);
}

bool TB_Move::strokeInit(mesh* refmesh, TweakPickInfo& pickInfo) {
	pick = pickInfo;
	mpick = pickInfo;
	mpick.origin.x = -mpick.origin.x;
	mpick.view.x = -mpick.view.x;
	mpick.facet = pickInfo.facetM;
	d = pick.origin.dot(pick.view);
	md = mpick.origin.dot(mpick.view);
	cachedFacets.clear();
	cachedNodes.clear();
	cachedPositions.clear();
	nCachedPoints = 0;
	nCachedPointsM = 0;

	if (cachedPoints) {
		free(cachedPoints);
	}
	cachedPoints = (int*)malloc(refmesh->nVerts * sizeof(int));

	if (!TweakBrush::queryPoints(refmesh, pick, cachedPoints, nCachedPoints, cachedFacets, cachedNodes))
		return false;

	for (int i = 0; i < nCachedPoints; i++)
		cachedPositions[cachedPoints[i]] = refmesh->verts[cachedPoints[i]];

	if (bMirror) {

		if (cachedPointsM) {
			free(cachedPointsM);
		}
		cachedPointsM = (int*)malloc(refmesh->nVerts * sizeof(int));
		cachedFacetsM.clear();
		cachedNodesM.clear();
		TweakBrush::queryPoints(refmesh, mpick, cachedPointsM, nCachedPointsM, cachedFacetsM, cachedNodesM);

		for (int i = 0; i < nCachedPointsM; i++)
			cachedPositions[cachedPointsM[i]] = refmesh->verts[cachedPointsM[i]];
	}

	return true;
}

bool TB_Move::queryPoints(mesh* refmesh, TweakPickInfo& pickInfo, int* resultPoints, int& outResultCount, vector<int>& resultFacets, unordered_set<AABBTree::AABBTreeNode*>& affectedNodes) {
	if (nCachedPoints == 0)
		return false;

	if (resultPoints) {
		memcpy(resultPoints, cachedPoints, nCachedPoints * sizeof(int));
		memcpy(resultPoints + nCachedPoints, cachedPointsM, nCachedPointsM * sizeof(int));
	}

	outResultCount = nCachedPoints + nCachedPointsM;

	return true;
}

void TB_Move::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, unordered_map<int, Vector3>& movedpoints){
	float dist = pickInfo.origin.dot(pick.view) - d;
	Vector3 v = pickInfo.origin - pick.view*dist;
	Vector3 dv = v - pick.origin;

	Matrix4 xform;
	xform.Translate(dv*strength);
	Vector3 vs;
	Vector3 ve;
	Vector3 vf;

	if (bMirror) {
		Vector3 lmo;
		lmo.x = -pickInfo.origin.x;	lmo.y = pickInfo.origin.y; lmo.z = pickInfo.origin.z;
		dist = lmo.dot(mpick.view) - md;
		v = lmo - mpick.view*dist;
		dv = v - mpick.origin;
		Matrix4 xformMirror;
		xformMirror.Translate(dv*strength);

		for (int p = 0; p < nCachedPointsM; p++) {
			int i = cachedPointsM[p];
			vs = cachedPositions[i];
			movedpoints[i] = vs;
			ve = xformMirror * vs;
			ve -= vs;
			applyFalloff(ve, mpick.origin.DistanceTo(vs));
			if (refmesh->vcolors)
				ve = ve * (1.0f - refmesh->vcolors[i].x);
			vf = vs + ve;
			refmesh->verts[i] = (vf);
		}
	}

	for (int p = 0; p < nCachedPoints; p++) {
		int i = cachedPoints[p];
		vs = cachedPositions[i];
		movedpoints[i] = vs;
		ve = xform * vs;
		ve -= vs;
		applyFalloff(ve, pick.origin.DistanceTo(vs));
		if (refmesh->vcolors)
			ve = ve * (1.0f - refmesh->vcolors[i].x);
		vf = vs + ve;
		refmesh->verts[i] = (vf);
	}
}

void TB_Move::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, Vector3* movedpoints) {
	float dist = pickInfo.origin.dot(pick.view) - d;
	Vector3 v = pickInfo.origin - pick.view*dist;
	Vector3 dv = v - pick.origin;

	Matrix4 xform;
	xform.Translate(dv*strength);
	Vector3 vs;
	Vector3 ve;
	Vector3 vf;

	if (bMirror) {
		Vector3 lmo;
		lmo.x = -pickInfo.origin.x;	lmo.y = pickInfo.origin.y; lmo.z = pickInfo.origin.z;
		dist = lmo.dot(mpick.view) - md;
		v = lmo - mpick.view*dist;
		dv = v - mpick.origin;
		Matrix4 xformMirror;
		xformMirror.Translate(dv*strength);

		for (int p = 0; p < nCachedPointsM; p++) {
			int i = cachedPointsM[p];
			vs = cachedPositions[i];
			movedpoints[p + nCachedPoints] = vs;
			ve = xformMirror * vs;
			ve -= vs;
			applyFalloff(ve, mpick.origin.DistanceTo(vs));
			if (refmesh->vcolors)
				ve = ve * (1.0f - refmesh->vcolors[i].x);
			vf = vs + ve;
			refmesh->verts[i] = (vf);
		}
	}

	for (int p = 0; p < nCachedPoints; p++) {
		int i = cachedPoints[p];
		vs = cachedPositions[i];
		movedpoints[p] = vs;
		ve = xform * vs;
		ve -= vs;
		applyFalloff(ve, pick.origin.DistanceTo(vs));
		if (refmesh->vcolors)
			ve = ve * (1.0f - refmesh->vcolors[i].x);
		vf = vs + ve;
		refmesh->verts[i] = (vf);
	}
}

void TB_Move::GetWorkingPlane(Vector3& outPlaneNormal, float& outPlaneDist) {
	outPlaneNormal = pick.view;
	outPlaneDist = d;
}

TB_XForm::TB_XForm() {
	brushType = TBT_XFORM;
	brushName = "Transform Brush";
	bLiveBVH = false;
	bLiveNormals = false;
	focus = 1.0f;
	strength = 1.0f;
	cachedPositions = nullptr;
	xformType = 0;
}

TB_XForm::~TB_XForm() {
	if (cachedPositions) {
		free(cachedPositions);
	}
}

void TB_XForm::GetWorkingPlane(Vector3& outPlaneNormal, float& outPlaneDist) {
	outPlaneNormal = pick.normal;
	outPlaneDist = pick.origin.dot(pick.normal);
}

bool TB_XForm::strokeInit(mesh* refmesh, TweakPickInfo& pickInfo) {
	pick = pickInfo;
	d = pick.origin.dot(pick.normal);

	cachedPositions;

	if (cachedPositions) {
		free(cachedPositions);
	}
	cachedPositions = (Vector3*)malloc(refmesh->nVerts * sizeof(Vector3));
	nCachedPoints = refmesh->nVerts;
	for (int i = 0; i < nCachedPoints; i++)	{
		cachedPositions[i] = refmesh->verts[i];
	}
	return true;
}

bool TB_XForm::queryPoints(mesh* refmesh, TweakPickInfo& pickInfo, int* resultPoints, int& outResultCount, vector<int>& resultFacets, unordered_set<AABBTree::AABBTreeNode*>& affectedNodes) {
	if (nCachedPoints == 0)
		return false;
	if (resultPoints) {
		for (int i = 0; i < refmesh->nVerts; i++)	{
			resultPoints[i] = i;
		}
	}
	outResultCount = refmesh->nVerts;
	return true;
}

void TB_XForm::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, unordered_map<int, Vector3>& movedpoints) {
	float dist = pickInfo.origin.dot(pick.normal) - d;
	Vector3 v = pickInfo.origin;
	v -= pick.view * dist;
	Vector3 dv = v - pick.origin;
	dv.x *= pick.view.x;
	dv.y *= pick.view.y;
	dv.z *= pick.view.z;

	Matrix4 xform;
	xform.Rotate(2.0f * PI * 10.0f / dv.x, pick.normal.x, pick.normal.y, pick.normal.z);
	Vector3 vs;
	Vector3 ve;
	Vector3 vf;

	for (int p = 0; p < nCachedPoints; p++) {
		vs = cachedPositions[p];
		movedpoints[p] = vs;
		ve = xform * vs;
		ve -= vs;
		if (refmesh->vcolors)
			ve = ve * (1.0f - refmesh->vcolors[p].x);
		vf = vs + ve;
		refmesh->verts[p] = (vf);
	}
}

void TB_XForm::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, Vector3* movedpoints) {
	Vector3 v = pickInfo.origin;
	Vector3 dv = v - pick.origin;

	dv.x *= pick.view.x;
	dv.y *= pick.view.y;
	dv.z *= pick.view.z;

	Matrix4 xform;
	float dist = 0.0f;
	if (fabs(dv.x) > EPSILON) {
		dist = dv.x;
	}
	else if (fabs(dv.y) > EPSILON) {
		dist = dv.y;
	}
	else if (fabs(dv.z) > EPSILON) {
		dist = dv.z;
	}


	if (xformType == 0) {
		xform.Translate(dv * strength);
	}
	else if (xformType == 1) {
		xform.PushTranslate(pick.center);
		Vector3 a = pick.origin - pick.center;
		Vector3 b = pickInfo.origin - pick.center;
		Vector3 dir = a.cross(b);
		float sign = dir.dot(pick.normal);
		float angle = a.angle(pickInfo.origin - pick.center);
		if (sign < 0)
			angle = -angle;

		xform.PushRotate(angle, pick.normal);
		xform.PushTranslate(pick.center * -1.0f);
	}
	else if (xformType == 2) {
		dist = fabs(1.0f + dist / 10.0f);
		xform.Scale(dist, dist, dist);
	}

	Vector3 vs;
	Vector3 ve;
	Vector3 vf;

	for (int p = 0; p < nCachedPoints; p++) {
		vs = cachedPositions[p];
		movedpoints[p] = vs;
		ve = xform * vs;
		ve -= vs;
		if (refmesh->vcolors)
			ve = ve * (1.0f - refmesh->vcolors[p].x);
		vf = vs + ve;
		refmesh->verts[p] = (vf);
	}
}

TB_Weight::TB_Weight() :TweakBrush() {
	brushType = TBT_WEIGHT;
	strength = 0.0015f;
	bFixedWeight = false;
	brushName = "Weight Paint";
}

TB_Weight::~TB_Weight() {
}

void TB_Weight::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, unordered_map<int, Vector3>& movedpoints) {
	Vector3 vs;
	Vector3 vc;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i < nPoints; i++) {
		vs = refmesh->verts[points[i]];
		vc = refmesh->vcolors[points[i]];
		movedpoints[points[i]] = vc;

		ve = vc;
		ve.y += strength;
		ve -= vc;

		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		ve = ve * (1.0f - refmesh->vcolors[points[i]].x);
		vf = vc + ve;
		if (vf.y > 1.0f) vf.y = 1.0f;
		refmesh->vcolors[points[i]] = vf;
	}
}

void TB_Weight::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, Vector3* movedpoints) {
	Vector3 vs;
	Vector3 vc;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i < nPoints; i++) {
		vs = refmesh->verts[points[i]];
		vc = refmesh->vcolors[points[i]];
		movedpoints[i] = vc;

		ve = vc;
		bFixedWeight ? ve.y = strength * 10.0f : ve.y += strength;
		ve -= vc;

		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		ve = ve * (1.0f - refmesh->vcolors[points[i]].x);
		vf = vc + ve;
		if (vf.y > 1.0f) vf.y = 1.0f;
		refmesh->vcolors[points[i]] = vf;
	}
}

TB_Unweight::TB_Unweight() :TweakBrush() {
	brushType = TBT_WEIGHT;
	strength = -0.0015f;
	brushName = "Weight Erase";
}

TB_Unweight::~TB_Unweight() {
}

void TB_Unweight::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, unordered_map<int, Vector3>& movedpoints) {
	Vector3 vs;
	Vector3 vc;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i < nPoints; i++) {
		vs = refmesh->verts[points[i]];
		vc = refmesh->vcolors[points[i]];
		movedpoints[points[i]] = vc;

		ve = vc;
		ve.y += strength;
		ve -= vc;

		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		ve = ve * (1.0f - refmesh->vcolors[points[i]].x);
		vf = vc + ve;
		if (vf.y < 0.0f) vf.y = 0.0f;
		refmesh->vcolors[points[i]] = vf;
	}
}

void TB_Unweight::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, Vector3* movedpoints) {
	Vector3 vs;
	Vector3 vc;
	Vector3 ve;
	Vector3 vf;

	for (int i = 0; i < nPoints; i++) {
		vs = refmesh->verts[points[i]];
		vc = refmesh->vcolors[points[i]];
		movedpoints[i] = vc;

		ve = vc;
		ve.y += strength;
		ve -= vc;

		applyFalloff(ve, pickInfo.origin.DistanceTo(vs));
		ve = ve * (1.0f - refmesh->vcolors[points[i]].x);
		vf = vc + ve;
		if (vf.y < 0.0f) vf.y = 0.0f;
		refmesh->vcolors[points[i]] = vf;
	}
}

TB_SmoothWeight::TB_SmoothWeight() :TweakBrush() {
	brushType = TBT_WEIGHT;
	strength = 0.015f;
	iterations = 1;
	method = 1;
	hcAlpha = 0.2f;
	hcBeta = 0.5f;
	bMirror = false;
	b = nullptr;
	lastMesh = nullptr;
	brushName = "Weight Smooth";
}

TB_SmoothWeight::~TB_SmoothWeight() {
	if (b) {
		free(b);
		b = nullptr;
	}
}

void TB_SmoothWeight::lapFilter(mesh* refmesh, int* points, int nPoints, unordered_map <int, Vector3>& wv) {
	unordered_map<int, Vector3>::iterator mi;
	Vector3 d;
	int adjPoints[1000];
	int c = 0;
	int a;

	for (int i = 0; i < nPoints; i++) {
		c = refmesh->GetAdjacentPoints(points[i], adjPoints, 1000);
		if (c == 0) continue;
		d.x = d.y = d.z = 0;
		// average adjacent points positions.  Since we're storing the changed vertices separately,
		// there's additional complexity involved with making sure we're grabbing a changed vertex 
		// instead of the original. This primarily comes into play when more than one iteration is used.
		for (int n = 0; n < c; n++) {
			a = adjPoints[n];
			mi = wv.find(a);
			if (mi != wv.end()) {
				d += mi->second;
			}
			else {
				d += refmesh->vcolors[a];
			}
		}
		wv[points[i]] = d / (float)c;

		if (refmesh->weldVerts.find(points[i]) != refmesh->weldVerts.end()) {
			for (unsigned int v = 0; v < refmesh->weldVerts[points[i]].size(); v++) {
				wv[refmesh->weldVerts[points[i]][v]] = wv[points[i]];
			}
		}
	}
}

void TB_SmoothWeight::hclapFilter(mesh* refmesh, int* points, int nPoints, unordered_map <int, Vector3>& wv) {
	unordered_map<int, Vector3>::iterator mi;

	if (refmesh != lastMesh) {
		if (b) free(b);
		b = (Vector3*)calloc(refmesh->nVerts, sizeof(Vector3));
		lastMesh = refmesh;
	}
	else {
		memset(b, 0, refmesh->nVerts * sizeof(Vector3));
	}

	Vector3 d;
	Vector3 q;
	int adjPoints[1000];
	int a;
	int c;
	int i;
	// First step is to calculate the laplacian
	for (int p = 0; p < nPoints; p++) {
		i = points[p];
		c = refmesh->GetAdjacentPoints(i, adjPoints, 1000);
		if (c == 0) continue;
		d.x = d.y = d.z = 0;
		// average adjacent points positions.  Since we're storing the changed vertices separately,
		// there's additional complexity involved with making sure we're grabbing a changed vertex 
		// instead of the original. This primarily comes into play when more than one iteration is used.
		for (int n = 0; n < c; n++) {
			a = adjPoints[n];
			mi = wv.find(a);
			if (mi != wv.end()) {
				d += mi->second;
			}
			else {
				d += refmesh->vcolors[a];
			}
		}
		// Save the current point's working position (or original if the working value hasn't been calculated
		// yet.)  This is used as part of the blend between original and changed position
		mi = wv.find(i);
		if (mi != wv.end())
			q = wv[i];
		else
			q = refmesh->vcolors[i];

		wv[i] = d / (float)c;
		// Calculate the difference between the new position and a blend of the original and previous positions
		b[i] = wv[i] - ((refmesh->vcolors[i] * hcAlpha) + (q * (1.0f - hcAlpha)));

		if (refmesh->weldVerts.find(i) != refmesh->weldVerts.end()) {
			for (unsigned int v = 0; v < refmesh->weldVerts[i].size(); v++) {
				wv[refmesh->weldVerts[i][v]] = wv[i];
			}
		}
	}

	for (int p = 0; p < nPoints; p++) {
		int j = points[p];
		c = refmesh->GetAdjacentPoints(j, adjPoints, 1000);
		if (c == 0) continue;
		d.x = d.y = d.z = 0;
		for (int n = 0; n < c; n++)
			d += b[adjPoints[n]];

		// blend the new position and the average of the distance moved
		float avgB = (1 - hcBeta) / (float)c;
		wv[j] -= ((b[j] * hcBeta) + (d * avgB));

		if (refmesh->weldVerts.find(j) != refmesh->weldVerts.end()) {
			for (unsigned int v = 0; v < refmesh->weldVerts[j].size(); v++) {
				wv[refmesh->weldVerts[j][v]] = wv[j];
			}
		}
	}
}

void TB_SmoothWeight::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, unordered_map<int, Vector3>& movedpoints) {
	unordered_map<int, Vector3> wv;
	Vector3 vs;
	Vector3 vc;

	for (int i = 0; i < nPoints; i++) {
		movedpoints[points[i]] = refmesh->vcolors[points[i]];
		wv[points[i]] = movedpoints[points[i]];
	}

	for (int iter = 0; iter < iterations; iter++) {
		if (method == 0)		// laplacian smooth
			lapFilter(refmesh, points, nPoints, wv);
		else					// HC-laplacian smooth
			hclapFilter(refmesh, points, nPoints, wv);
	}

	Vector3 delta;
	if (strength != 1.0f) {
		for (int p = 0; p < nPoints; p++) {
			int i = points[p];
			vs = refmesh->verts[i];
			vc = refmesh->vcolors[i];
			delta.y = wv[i].y - vc.y;
			delta.y *= strength;

			applyFalloff(delta, pickInfo.origin.DistanceTo(vs));

			if (refmesh->vcolors)
				delta.y *= 1.0f - refmesh->vcolors[i].x;

			vc.y += delta.y;

			if (vc.y < EPSILON) vc.y = 0.0f;
			if (vc.y > 1.0f) vc.y = 1.0f;
			refmesh->vcolors[i].y = vc.y;
		}
	}
}

void TB_SmoothWeight::brushAction(mesh* refmesh, TweakPickInfo& pickInfo, int* points, int nPoints, Vector3* movedpoints) {
	unordered_map<int, Vector3> wv;
	Vector3 vs;
	Vector3 vc;

	for (int i = 0; i < nPoints; i++) {
		movedpoints[i] = refmesh->vcolors[points[i]];
		wv[points[i]] = movedpoints[i];
	}

	for (int iter = 0; iter < iterations; iter++) {
		if (method == 0)		// laplacian smooth
			lapFilter(refmesh, points, nPoints, wv);
		else					// HC-laplacian smooth
			hclapFilter(refmesh, points, nPoints, wv);
	}

	Vector3 delta;
	if (strength != 1.0f) {
		for (int p = 0; p < nPoints; p++) {
			int i = points[p];
			vs = refmesh->verts[i];
			vc = refmesh->vcolors[i];
			delta.y = wv[i].y - vc.y;
			delta.y *= strength;

			applyFalloff(delta, pickInfo.origin.DistanceTo(vs));

			if (refmesh->vcolors)
				delta.y *= 1.0f - refmesh->vcolors[i].x;

			vc.y += delta.y;

			if (vc.y < EPSILON) vc.y = 0.0f;
			if (vc.y > 1.0f) vc.y = 1.0f;
			refmesh->vcolors[i].y = vc.y;
		}
	}
}
