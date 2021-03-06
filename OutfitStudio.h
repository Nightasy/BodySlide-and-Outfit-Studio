/*
BodySlide and Outfit Studio
Copyright(C) 2015  Caliente & ousnius

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "stdafx.h"
#include "wxStateButton.h"
#include "GLSurface.h"
#include "SliderData.h"
#include "SliderPresets.h"
#include "ObjFile.h"
#include "TweakBrush.h"
#include "Automorph.h"
#include "OutfitProject.h"
#include "ConfigurationManager.h"

#include <wx/xrc/xmlres.h>
#include <wx/treectrl.h>
#include <wx/wizard.h>
#include <wx/filepicker.h>
#include <wx/grid.h>
#include <wx/progdlg.h>
#include <wx/spinctrl.h>
#include <wx/dataview.h>
#include <wx/splitter.h>
#include <wx/collpane.h>

enum TargetGame {
	FO3, FONV, SKYRIM, FO4
};


class ShapeItemData : public wxTreeItemData  {
public:
	NifFile* refFile;
	string shapeName;
	ShapeItemData(NifFile* inRefFile = nullptr, const string& inShapeName = "") {
		refFile = inRefFile;
		shapeName = inShapeName;
	}
};


class wxGLPanel : public wxGLCanvas {
public:
	wxGLPanel(wxWindow* parent, const wxSize& size, const int* attribs);
	~wxGLPanel();

	void SetNotifyWindow(wxWindow* win);

	void AddMeshFromNif(NifFile* nif, const string& shapeName, bool buildNormals);
	void AddExplicitMesh(vector<Vector3>* v, vector<Triangle>* t, vector<Vector2>* uv = nullptr, const string& shapeName = "");

	void RenameShape(const string& shapeName, const string& newShapeName) {
		gls.RenameMesh(shapeName, newShapeName);
	}

	void SetMeshTexture(const string& shapeName, const string& texturefile, bool isSkin = false);

	mesh* GetMesh(const string& shapeName) {
		return gls.GetMesh(shapeName);
	}

	void UpdateMeshVertices(const string& shapeName, vector<Vector3>* verts, bool updateBVH = true, bool recalcNormals = true);
	void RecalculateMeshBVH(const string& shapeName);

	void ShowShape(const string& shapeName, bool show = true);
	void SetActiveShape(const string& shapeName);

	TweakUndo* GetStrokeManager() {
		return strokeManager;
	}
	void SetStrokeManager(TweakUndo* manager) {
		if (!manager)
			strokeManager = &baseStrokes;
		else
			strokeManager = manager;
	}

	void SetActiveBrush(int brushID);
	TweakBrush* GetActiveBrush() {
		return activeBrush;
	}

	bool StartBrushStroke(const wxPoint& screenPos);
	void UpdateBrushStroke(const wxPoint& screenPos);
	void EndBrushStroke();

	bool StartTransform(const wxPoint& screenPos);
	void UpdateTransform(const wxPoint& screenPos);
	void EndTransform();

	bool UndoStroke();
	bool RedoStroke();

	void ShowTransformTool(bool show = true, bool updateBrush = true);

	void SetEditMode(bool on = true) {
		editMode = on;
	}
	void ToggleEditMode() {
		if (transformMode != 0) {
			transformMode = 0;
			editMode = true;
			ShowTransformTool(false);
		}
		else {
			editMode = false;
			transformMode = 1;
			ShowTransformTool();
		}
	}
	bool GetXMirror() {
		return bXMirror;
	}
	void SetXMirror(bool on = true) {
		bXMirror = on;
	}
	void ToggleXMirror() {
		if (bXMirror)
			bXMirror = false;
		else
			bXMirror = true;
	}
	void ToggleConnectedEdit() {
		if (bConnectedEdit)
			bConnectedEdit = false;
		else
			bConnectedEdit = true;
	}

	void SetShapeGhostMode(const string& shapeName, bool on = true) {
		mesh* m = gls.GetMesh(shapeName);
		if (!m) return;
		if (on)
			m->rendermode = RenderMode::LitWire;
		else
			m->rendermode = RenderMode::Normal;
	}
	void ToggleGhostMode() {
		mesh* m = gls.GetActiveMesh();
		if (!m)
			return;

		if (m->rendermode == RenderMode::Normal)
			m->rendermode = RenderMode::LitWire;
		else if (m->rendermode == RenderMode::LitWire)
			m->rendermode = RenderMode::Normal;
	}

	void ToggleNormalSeamSmoothMode() {
		mesh* m = gls.GetActiveMesh();
		if (!m)
			return;

		if (m->smoothSeamNormals == true)
			m->smoothSeamNormals = false;
		else
			m->smoothSeamNormals = true;
		m->SmoothNormals();
	}

	void RecalcNormals(const string& shape, bool forceSmoothSeam = false) {
		mesh* m = gls.GetMesh(shape);
		if (!m)
			return;
		if (forceSmoothSeam) {
			m->smoothSeamNormals = true;
			m->SmoothNormals();
			m->smoothSeamNormals = false;
		}
	}
	void ToggleAutoNormals() {
		if (bAutoNormals)
			bAutoNormals = false;
		else
			bAutoNormals = true;
	}
	float GetBrushSize() {
		return brushSize / 3.0f;
	}
	void SetBrushSize(float val) {
		val *= 3.0f;
		brushSize = val;
		gls.SetCursorSize(val);
	}

	float IncBrush() {
		gls.SetCursorSize(gls.GetCursorSize() + 0.010f);
		brushSize += 0.010f;
		LimitBrushSize();
		return brushSize;
	}
	float DecBrush() {
		gls.SetCursorSize(gls.GetCursorSize() - 0.010f);
		brushSize -= 0.010f;
		LimitBrushSize();
		return brushSize;
	}
	void LimitBrushSize() {
		if (brushSize < 0.000f) {
			gls.SetCursorSize(0.000f);
			brushSize = 0.000f;
		}
		else if (brushSize > 3.000f) {
			gls.SetCursorSize(3.000f);
			brushSize = 3.000f;
		}
	}

	float IncStr() {
		if (!activeBrush)
			return 0.0f;

		float str = activeBrush->getStrength();
		str += 0.010f;
		activeBrush->setStrength(str);
		return str;
	}
	float DecStr() {
		if (!activeBrush)
			return 0.0f;

		float str = activeBrush->getStrength();
		str -= 0.010f;
		activeBrush->setStrength(str);
		return str;
	}

	void ShowWireframe() {
		gls.ToggleWireframe();
	}
	void ToggleLighting() {
		gls.ToggleLighting();
	}
	void ToggleTextures() {
		gls.ToggleTextures();
	}
	void ToggleMaskVisible() {
		mesh* m = gls.GetActiveMesh();
		if (!m->vcolors) {
			m->vcolors = new Vector3[m->nVerts];
			memset(m->vcolors, 0, 12 * m->nVerts);
		}
		gls.ToggleMask();
	}
	void SetWeightVisible(bool bVisible = true) {
		gls.SetWeightColors(bVisible);
	}

	void ClearMask() {
		mesh* m = gls.GetActiveMesh();
		if (m->vcolors)
			m->ColorChannelFill(0, 0.0f);
	}

	void GetActiveMask(unordered_map<ushort, float>& mask) {
		mesh* m = gls.GetActiveMesh();
		if (!m->vcolors)
			return;

		for (int i = 0; i < m->nVerts; i++) {
			if (m->vcolors[i].x != 0.0f)
				mask[i] = m->vcolors[i].x;
		}
	}

	void GetActiveUnmasked(unordered_map<ushort, float>& mask) {
		mesh* m = gls.GetActiveMesh();
		if (!m->vcolors) {
			for (int i = 0; i < m->nVerts; i++)
				mask[i] = 0.0f;
			return;
		}
		for (int i = 0; i < m->nVerts; i++)
			if (m->vcolors[i].x == 0.0f)
				mask[i] = m->vcolors[i].x;
	}

	void GetShapeMask(unordered_map<ushort, float>& mask, const string& shapeName) {
		mesh* m = gls.GetMesh(shapeName);
		if (!m || !m->vcolors)
			return;

		for (int i = 0; i < m->nVerts; i++) {
			if (m->vcolors[i].x != 0.0f)
				mask[i] = m->vcolors[i].x;
		}
	}

	void GetShapeUnmasked(unordered_map<ushort, float>& mask, const string& shapeName) {
		mesh* m = gls.GetMesh(shapeName);
		if (!m)
			return;

		if (!m->vcolors) {
			for (int i = 0; i < m->nVerts; i++)
				mask[i] = 0.0f;
			return;
		}
		for (int i = 0; i < m->nVerts; i++)
			if (m->vcolors[i].x == 0.0f)
				mask[i] = m->vcolors[i].x;
	}

	void InvertMask() {
		mesh* m = gls.GetActiveMesh();
		if (!m->vcolors)
			m->ColorFill(Vector3(0.0f, 0.0f, 0.0f));
		for (int i = 0; i < m->nVerts; i++)
			m->vcolors[i].x = 1 - m->vcolors[i].x;
	}

	void DeleteMesh(const string& shape) {
		gls.DeleteMesh(shape);
		Refresh();
	}
	void DestroyOverlays() {
		gls.DeleteOverlays();
		Refresh();
	}

	void SetView(const char& type) {
		gls.SetView(type);
		Refresh();
	}

	void SetPerspective(const bool& enabled) {
		gls.SetPerspective(enabled);
		Refresh();
	}

	void SetFieldOfView(const int& fieldOfView) {
		gls.SetFieldOfView(fieldOfView);
		Refresh();
	}

	void UpdateLights(const int& ambient = 50, const int& brightness1 = 50, const int& brightness2 = 50, const int& brightness3 = 50) {
		gls.UpdateLights(ambient, brightness1, brightness2, brightness3);
		Refresh();
	}


private:
	void OnShown();
	void OnPaint(wxPaintEvent& event);
	void OnSize(wxSizeEvent& event);

	void OnMouseWheel(wxMouseEvent& event);
	void OnMouseMove(wxMouseEvent& event);

	void OnMiddleDown(wxMouseEvent& event);
	void OnMiddleUp(wxMouseEvent& event);
	void OnLeftDown(wxMouseEvent& event);
	void OnLeftUp(wxMouseEvent& event);

	void OnRightDown(wxMouseEvent& event);
	void OnRightUp(wxMouseEvent& event);

	void OnKeys(wxKeyEvent& event);
	void OnIdle(wxIdleEvent& event);

	void OnCaptureLost(wxMouseCaptureLostEvent& event);

	GLSurface gls;
	wxGLContext* context;

	bool rbuttonDown;
	bool lbuttonDown;
	bool mbuttonDown;
	bool isLDragging;
	bool isMDragging;
	bool isRDragging;
	bool firstPaint{true};

	int lastX;
	int lastY;

	set<int> BVHUpdateQueue;

	wxWindow* notifyWindow;

	float brushSize;
	bool editMode;
	bool transformMode;			// 0 = off, 1 = move, 2 = rotate, 3 = scale
	bool bMaskPaint;
	bool bWeightPaint;
	bool isPainting;
	bool isTransforming;
	bool bXMirror;
	bool bAutoNormals;
	bool bConnectedEdit;

	TweakBrush* activeBrush;
	TweakBrush* savedBrush;
	TweakBrush standardBrush;
	TB_Deflate deflateBrush;
	TB_Move moveBrush;
	TB_Smooth smoothBrush;
	TB_Mask maskBrush;
	TB_Unmask UnMaskBrush;
	TB_Weight weightBrush;
	TB_Unweight unweightBrush;
	TB_SmoothWeight smoothWeightBrush;
	TB_XForm translateBrush;

	TweakStroke* activeStroke;
	TweakUndo* strokeManager;
	TweakUndo baseStrokes;

	Vector3 xformCenter;		// Transform center for transform brushes (rotate, specifically cares about this)

	DECLARE_EVENT_TABLE()
};

class OutfitProject;

class OutfitStudio : public wxFrame {
public:
	OutfitStudio(wxWindow* parent, const wxPoint& pos, const wxSize& size, ConfigurationManager& inConfig);
	~OutfitStudio();

	int targetGame;
	wxGLPanel* glView = nullptr;
	OutfitProject* project = nullptr;
	ShapeItemData* activeItem = nullptr;
	vector<ShapeItemData*> selectedItems;
	string activeSlider;
	string activeBone;
	bool bEditSlider;

	wxTreeCtrl* outfitShapes;
	wxTreeCtrl* outfitBones;
	wxPanel* lightSettings;
	wxSlider* boneScale;
	wxScrolledWindow* sliderScroll;
	wxStatusBar* statusBar;
	wxToolBar* toolBar;
	wxTreeItemId shapesRoot;
	wxTreeItemId outfitRoot;
	wxTreeItemId bonesRoot;
	wxImageList* visStateImages;

	ConfigurationManager& appConfig;

	class SliderDisplay {
	public:
		bool hilite;
		wxPanel* sliderPane;
		wxBoxSizer* paneSz;

		int sliderNameCheckID;
		int sliderID;

		wxBitmapButton* btnSliderEdit;
		wxButton* btnMinus;
		wxButton* btnPlus;
		wxCheckBox* sliderNameCheck;
		wxStaticText* sliderName;
		wxSlider* slider;
		wxTextCtrl* sliderReadout;

		TweakUndo sliderStrokes;			// This probably shouldn't be here, but it's a convenient location to store undo info.
	};

	map<string, SliderDisplay*> sliderDisplays;

	void CreateSetSliders();

	string NewSlider(const string& suggestedName = "", bool skipPrompt = false);

	void SetSliderValue(int index, int val);
	void SetSliderValue(const string& name, int val);

	void ApplySliders(bool recalcBVH = true);

	void ShowSliderEffect(int slider, bool show = true);
	void ShowSliderEffect(const string& sliderName, bool show = true);

	void SelectShape(const string& shapeName);
	vector<string> GetShapeList();

	void UpdateShapeSource(const string& shapeName);
	int PromptUpdateBase();

	void ActiveShapeUpdated(TweakStroke* refStroke, bool bIsUndo = false, bool setWeights = true);
	void UpdateActiveShapeUI();

	void RefreshGUIFromProj();

	string GetActiveBone();

	bool NotifyStrokeStarting();

	bool IsDirty();
	bool IsDirty(const string& shapeName);
	void SetClean(const string& shapeName);

	void EnterSliderEdit(const string& sliderName);
	void ExitSliderEdit();
	void MenuEnterSliderEdit();
	void MenuExitSliderEdit();

	void ToggleBrushPane() {
		wxCollapsiblePane* brushPane = (wxCollapsiblePane*)FindWindowByName("brushPane");
		if (!brushPane)
			return;

		brushPane->Collapse(!brushPane->IsCollapsed());

		wxWindow* leftPanel = FindWindowByName("leftSplitPanel");
		if (leftPanel)
			leftPanel->Layout();
	}

	void UpdateBrushPane() {
		TweakBrush* brush = glView->GetActiveBrush();
		if (!brush)
			return;

		wxCollapsiblePane* parent = (wxCollapsiblePane*)FindWindowByName("brushPane");
		if (!parent)
			return;

		XRCCTRL(*parent, "brushSize", wxSlider)->SetValue(glView->GetBrushSize() * 1000.0f);
		wxStaticText* valSize = (wxStaticText*)XRCCTRL(*parent, "valSize", wxStaticText);
		wxString valSizeStr = wxString::Format("%0.3f", glView->GetBrushSize());
		valSize->SetLabel(valSizeStr);

		XRCCTRL(*parent, "brushStr", wxSlider)->SetValue(brush->getStrength() * 1000.0f);
		wxStaticText* valStr = (wxStaticText*)XRCCTRL(*parent, "valStr", wxStaticText);
		wxString valStrengthStr = wxString::Format("%0.3f", brush->getStrength());
		valStr->SetLabel(valStrengthStr);

		XRCCTRL(*parent, "brushFocus", wxSlider)->SetValue(brush->getFocus() * 1000.0f);
		wxStaticText* valFocus = (wxStaticText*)XRCCTRL(*parent, "valFocus", wxStaticText);
		wxString valFocusStr = wxString::Format("%0.3f", brush->getFocus());
		valFocus->SetLabel(valFocusStr);

		XRCCTRL(*parent, "brushSpace", wxSlider)->SetValue(brush->getSpacing() * 1000.0f);
		wxStaticText* valSpace = (wxStaticText*)XRCCTRL(*parent, "valSpace", wxStaticText);
		wxString valSpacingStr = wxString::Format("%0.3f", brush->getSpacing());
		valSpace->SetLabel(valSpacingStr);
	}

	void CheckBrushBounds() {
		TweakBrush* brush = glView->GetActiveBrush();
		if (!brush)
			return;

		float size = glView->GetBrushSize();
		float strength = brush->getStrength();
		//float focus = brush->getFocus();
		//float spacing = brush->getSpacing();

		if (size >= 1.0f)
			GetMenuBar()->Enable(XRCID("btnIncreaseSize"), false);
		else
			GetMenuBar()->Enable(XRCID("btnIncreaseSize"), true);

		if (size <= 0.0f)
			GetMenuBar()->Enable(XRCID("btnDecreaseSize"), false);
		else
			GetMenuBar()->Enable(XRCID("btnDecreaseSize"), true);

		if (strength >= 1.0f)
			GetMenuBar()->Enable(XRCID("btnIncreaseStr"), false);
		else
			GetMenuBar()->Enable(XRCID("btnIncreaseStr"), true);

		if (strength <= 0.0f)
			GetMenuBar()->Enable(XRCID("btnDecreaseStr"), false);
		else
			GetMenuBar()->Enable(XRCID("btnDecreaseStr"), true);
	}

	wxProgressDialog* progWnd;
	vector<pair<float, float>> progressStack;
	int progressVal;

	void StartProgress(const wxString& title) {
		if (progressStack.empty()) {
			progWnd = new wxProgressDialog(title, "Starting...", 10000, this, wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_SMOOTH | wxPD_ELAPSED_TIME);
			progWnd->SetSize(400, 150);
			progressVal = 0;
			progressStack.emplace_back(0.0f, 10000.0f);
		}
	}

	void StartSubProgress(float min, float max) {
		float range = progressStack.back().second - progressStack.front().first;
		float mindiv = min / 100.0f;
		float maxdiv = max / 100.0f;
		float minoff = mindiv * range;
		float maxoff = maxdiv * range;
		progressStack.emplace_back(progressStack.front().first + minoff, progressStack.front().first + maxoff);
	}

	void EndProgress() {
		if (progressStack.empty())
			return;

		progWnd->Update(progressStack.back().second);
		progressStack.pop_back();

		if (progressStack.empty()) {
			delete progWnd;
			progWnd = nullptr;
		}
	}

	void UpdateProgress(float val, const wxString& msg = "") {
		if (progressStack.empty())
			return;

		float range = progressStack.back().second - progressStack.back().first;
		float div = val / 100.0f;
		float offset = range * div;

		progressVal = progressStack.back().first + offset;
		if (progressVal > 10000)
			progressVal = 10000;

		progWnd->Update(progressVal, msg);
	}


	bool shapeIsImporting(const string&  shapeName) {
		auto ss = shapeStates.find(shapeName);
		if (ss != shapeStates.end()) {
			return ss->second.bIsImporting;
		}
		return false;
	}
	void setShapeImporting(const string& shapeName, bool isOn) {
		shapeStates[shapeName].bIsImporting = isOn;
	}
	void finishImporting() {
		for (auto &ss : shapeStates) {
			ss.second.bIsImporting = false;
		}
	}

private:
	class ShapeState {						// metadata state of shapes by shape name
	public:
		TargetGame gameType;				// game type source (affects block types in nif)
		bool bIsImporting;					// true if shape is currently in the process of being imported from .obj (lacking normals, textures, etc.)
		ShapeState() : gameType(SKYRIM), bIsImporting(false) {}
	};

	bool previousMirror;
	Vector3 previewMove;
	float previewScale;
	Vector3 previewRotation;

	map<string, ShapeState> shapeStates;

	void createSliderGUI(const string& name, int id, wxScrolledWindow* wnd, wxSizer* rootSz);
	void HighlightSlider(const string& name);
	void ZeroSliders();

	void ClearProject();
	void RenameProject(const string& projectName);

	void AnimationGUIFromProj();
	void WorkingGUIFromProj();

	void OnMoveWindow(wxMoveEvent& event);
	void OnSetSize(wxSizeEvent& event);

	void OnExit(wxCommandEvent& event);
	void OnNewProject(wxCommandEvent& event);
	void OnLoadProject(wxCommandEvent &event);
	void OnLoadReference(wxCommandEvent &event);
	void OnLoadOutfit(wxCommandEvent& event);

	void OnNewProject2FP_NIF(wxFileDirPickerEvent& event);
	void OnNewProject2FP_OBJ(wxFileDirPickerEvent& event);
	void OnNewProject2FP_Texture(wxFileDirPickerEvent& event);
	void OnLoadOutfitFP_NIF(wxFileDirPickerEvent& event);
	void OnLoadOutfitFP_OBJ(wxFileDirPickerEvent& event);
	void OnLoadOutfitFP_Texture(wxFileDirPickerEvent& event);

	void OnSetBaseShape(wxCommandEvent &event);
	void OnExportOutfitNif(wxCommandEvent &event);
	void OnExportOutfitNifWithRef(wxCommandEvent &event);
	void OnMakeConvRef(wxCommandEvent& event);

	void OnSSSNameCopy(wxCommandEvent& event);
	void OnSSSGenWeightsTrue(wxCommandEvent& event);
	void OnSSSGenWeightsFalse(wxCommandEvent& event);
	void OnSaveSliderSet(wxCommandEvent &event);
	void OnSaveSliderSetAs(wxCommandEvent &event);

	void OnBrushPane(wxCollapsiblePaneEvent &event);

	void OnSlider(wxScrollEvent& event);
	void OnClickSliderButton(wxCommandEvent &event);
	void OnReadoutChange(wxCommandEvent& event);

	void OnTabButtonClick(wxCommandEvent& event);
	void OnFixedWeight(wxCommandEvent& event);
	void OnSelectSliders(wxCommandEvent& event);

	void OnShapeVisToggle(wxTreeEvent& event);
	void OnShapeSelect(wxTreeEvent& event);
	void OnShapeContext(wxTreeEvent& event);
	void OnShapeDrag(wxTreeEvent& event);
	void OnShapeDrop(wxTreeEvent& event);
	void OnBoneSelect(wxTreeEvent& event);
	void OnBoneContext(wxTreeEvent& event);
	void OnCheckTreeSel(wxTreeEvent& event);
	void OnCheckBox(wxCommandEvent& event);

	void OnSelectBrush(wxCommandEvent& event);

	void OnSetView(wxCommandEvent& event);
	void OnTogglePerspective(wxCommandEvent& event);
	void OnFieldOfViewSlider(wxCommandEvent& event);
	void OnUpdateLights(wxCommandEvent& event);
	void OnResetLights(wxCommandEvent& event);

	void OnLoadPreset(wxCommandEvent& event);
	void OnSliderConform(wxCommandEvent& event);
	void OnSliderConformAll(wxCommandEvent& event);
	void OnSliderImportBSD(wxCommandEvent& event);
	void OnSliderImportOBJ(wxCommandEvent& event);
	void OnSliderImportTRI(wxCommandEvent& event);
	void OnSliderImportFBX(wxCommandEvent& event);
	void OnSliderExportBSD(wxCommandEvent& event);
	void OnSliderExportOBJ(wxCommandEvent& event);
	void OnSliderExportTRI(wxCommandEvent& event);

	void OnNewSlider(wxCommandEvent& event);
	void OnNewZapSlider(wxCommandEvent& event);
	void OnNewCombinedSlider(wxCommandEvent& event);
	void OnSliderNegate(wxCommandEvent& event);
	void OnClearSlider(wxCommandEvent& event);
	void OnDeleteSlider(wxCommandEvent& event);
	void OnSliderProperties(wxCommandEvent& event);

	void OnImportShape(wxCommandEvent& event);
	void OnExportShape(wxCommandEvent& event);
	void OnImportFBX(wxCommandEvent& event);
	void OnExportFBX(wxCommandEvent& event);

	void OnEnterClose(wxKeyEvent& event);
	
	void OnMoveShape(wxCommandEvent& event);
	void OnMoveShapeOldOffset(wxCommandEvent& event);
	void OnMoveShapeSlider(wxCommandEvent& event);
	void OnMoveShapeText(wxCommandEvent& event);
	void PreviewMove(const Vector3& changed);

	void OnScaleShape(wxCommandEvent& event);
	void OnScaleShapeSlider(wxCommandEvent& event);
	void OnScaleShapeText(wxCommandEvent& event);
	void PreviewScale(const float& scale);

	void OnRotateShape(wxCommandEvent& event);
	void OnRotateShapeSlider(wxCommandEvent& event);
	void OnRotateShapeText(wxCommandEvent& event);
	void PreviewRotation(const Vector3& changed);

	void OnRenameShape(wxCommandEvent& event);
	void OnSetReference(wxCommandEvent& event);
	void OnDupeShape(wxCommandEvent& event);
	void OnDeleteShape(wxCommandEvent& event);
	void OnAddBone(wxCommandEvent& event);
	void OnDeleteBone(wxCommandEvent& event);
	void OnCopyBoneWeight(wxCommandEvent& event);
	void OnCopySelectedWeight(wxCommandEvent& event);
	void OnTransferSelectedWeight(wxCommandEvent& event);
	void OnMaskWeighted(wxCommandEvent& event);
	void OnBuildSkinPartitions(wxCommandEvent& event);
	void OnShapeProperties(wxCommandEvent& event);

	void OnNPWizChangeSliderSetFile(wxFileDirPickerEvent& event);
	void OnNPWizChangeSetNameChoice(wxCommandEvent& event);

	void OnBrushSettingsSlider(wxScrollEvent& event);

	void OnXMirror(wxCommandEvent& WXUNUSED(event)) {
		glView->ToggleXMirror();
	}

	void OnConnectedOnly(wxCommandEvent& WXUNUSED(event)) {
		glView->ToggleConnectedEdit();
	}

	void OnUndo(wxCommandEvent& WXUNUSED(event)) {
		glView->UndoStroke();
	}
	void OnRedo(wxCommandEvent& WXUNUSED(event)) {
		glView->RedoStroke();
	}

	void OnRecalcNormals(wxCommandEvent& WXUNUSED(event)) {
		glView->RecalcNormals(activeItem->shapeName);
		glView->Refresh();
	}

	void OnAutoNormals(wxCommandEvent& WXUNUSED(event)) {
		glView->ToggleAutoNormals();
		glView->Refresh();
	}

	void OnSmoothNormalSeams(wxCommandEvent& WXUNUSED(event)) {
		glView->ToggleNormalSeamSmoothMode();
		glView->Refresh();
	}

	void OnGhostMesh(wxCommandEvent& WXUNUSED(event)) {
		glView->ToggleGhostMode();
		glView->Refresh();
	}

	void OnShowWireframe(wxCommandEvent& WXUNUSED(event)) {
		glView->ShowWireframe();
		glView->Refresh();
	}

	void OnEnableLighting(wxCommandEvent& WXUNUSED(event)) {
		glView->ToggleLighting();
		glView->Refresh();
	}

	void OnEnableTextures(wxCommandEvent& WXUNUSED(event)) {
		glView->ToggleTextures();
		glView->Refresh();
	}

	void OnShowMask(wxCommandEvent& WXUNUSED(event)) {
		if (!activeItem)
			return;

		glView->ToggleMaskVisible();
		glView->Refresh();
	}

	void OnIncBrush(wxCommandEvent& WXUNUSED(event)) {
		if (glView->GetActiveBrush() && glView->GetBrushSize() < 1.0f) {
			float v = glView->IncBrush() / 3.0f;
			if (statusBar)
				statusBar->SetStatusText(wxString::Format("Rad: %f", v), 2);

			CheckBrushBounds();
			UpdateBrushPane();
		}
	}
	void OnDecBrush(wxCommandEvent& WXUNUSED(event)) {
		if (glView->GetActiveBrush() && glView->GetBrushSize() > 0.0f) {
			float v = glView->DecBrush() / 3.0f;
			if (statusBar)
				statusBar->SetStatusText(wxString::Format("Rad: %f", v), 2);

			CheckBrushBounds();
			UpdateBrushPane();
		}
	}
	void OnIncStr(wxCommandEvent& WXUNUSED(event)) {
		if (glView->GetActiveBrush() && glView->GetActiveBrush()->getStrength() < 1.0f) {
			float v = glView->IncStr();
			if (statusBar)
				statusBar->SetStatusText(wxString::Format("Str: %f", v), 2);

			CheckBrushBounds();
			UpdateBrushPane();
		}
	}
	void OnDecStr(wxCommandEvent& WXUNUSED(event)) {
		if (glView->GetActiveBrush() && glView->GetActiveBrush()->getStrength() > 0.0f) {
			float v = glView->DecStr();
			if (statusBar)
				statusBar->SetStatusText(wxString::Format("Str: %f", v), 2);

			CheckBrushBounds();
			UpdateBrushPane();
		}
	}

	void OnClearMask(wxCommandEvent& WXUNUSED(event)) {
		if (!activeItem)
			return;

		glView->ClearMask();
		glView->Refresh();
	}

	void OnInvertMask(wxCommandEvent& WXUNUSED(event)) {
		if (!activeItem)
			return;

		glView->InvertMask();
		glView->Refresh();
	}

	DECLARE_EVENT_TABLE()
};

class DnDFile : public wxFileDropTarget {
public:
	DnDFile(OutfitStudio *pOwner = nullptr) { owner = pOwner; }

	virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& fileNames);

private:
	OutfitStudio *owner;
};

class DnDSliderFile : public wxFileDropTarget {
public:
	DnDSliderFile(OutfitStudio *pOwner = nullptr) { owner = pOwner; }

	virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString& fileNames);
	virtual wxDragResult OnDragOver(wxCoord x, wxCoord y, wxDragResult defResult);

private:
	OutfitStudio *owner;
	wxDragResult lastResult;
	string targetSlider;
};
