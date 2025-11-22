#include "RdrState.h"
#include "WorldGrid.h"
#include "dynwind.h"
#include "Color.h"
#include "GlobalTypes.h"
#include "Materials.h"
#include "GfxMaterials.h"
#include "GfxTerrain.h"
#include "GfxCommonSnap.h"

#include "GfxMapSnap.h"
#include "MapSnap.h"

AUTO_RUN_ANON(memBudgetAddMapping(__FILE__, BUDGET_Renderer););

//set the flags that NNO wants for map snaps
void OVERRIDE_LATELINK_gfxMapSnapSetupOptions(WorldRegion *region)
{
	/*strcpy(g_strSimpleMaterials, "MapSnap_Override_NW_01");
	if( g_bMapSnapMaterialOverrideSimple ) {
		strcpy(g_strSimpleAlphaMaterials, "MapSnap_Override_NW_01");
	} else {
		strcpy(g_strSimpleAlphaMaterials, "Invisible");
	}*/
	
	if (!material_load_info.stTemplateOverrides)
	{
		material_load_info.stTemplateOverrides = stashTableCreateWithStringKeys(10, StashDefault);
	
		//specific template overrides:
		stashAddPointer(material_load_info.stTemplateOverrides,"Standard_Blend_High",materialGetTemplateByName("Mapsnap_Standard_Blend_High"),false);
		//stashAddPointer(material_load_info.stTemplateOverrides,"Terrainmaterial",materialGetTemplateByName("Mapsnap_Terrainmaterial"),false);
		stashAddPointer(material_load_info.stTemplateOverrides,"Terrain_3Detail",materialGetTemplateByName("Mapsnap_Terrain_3Detail"),false);
		stashAddPointer(material_load_info.stTemplateOverrides,"Terrain_2Detail",materialGetTemplateByName("Mapsnap_Terrain_2Detail"),false);
		stashAddPointer(material_load_info.stTemplateOverrides,"Terrain_1Detail",materialGetTemplateByName("Mapsnap_Terrain_1Detail"),false);
		stashAddPointer(material_load_info.stTemplateOverrides,"Terrain_0Detail",materialGetTemplateByName("Mapsnap_Terrain_0Detail"),false);
		stashAddPointer(material_load_info.stTemplateOverrides,"Standard_Liquid_VeryHigh",materialGetTemplateByName("Mapsnap_Water"),false);
		stashAddPointer(material_load_info.stTemplateOverrides,"Custom_Ocean_Simplified",materialGetTemplateByName("Mapsnap_Water"),false);
		//stashAddPointer(material_load_info.stTemplateOverrides,"Singletexture",materialGetTemplateByName("Mapsnap_Singletexture"),false);
		//stashAddPointer(material_load_info.stTemplateOverrides,"Diffuse_Colorspec_1normal_Overlay",materialGetTemplateByName("Mapsnap_Singletexture"),false);

		g_stTextureOpOverride = stashTableCreateWithStringKeys(5, StashDefault);
		stashAddPointer(g_stTextureOpOverride,"Mapsnap_Noisetexture","Mapsnap_Grit",false);

		gfxMaterialsReloadAll();
	}

	g_pchColorTexOverride = "Mapsnap_Grit";

	//set options for hiding obscuring details, outlining, and simple material overrides:
	gfxSnapApplyOptions(true, true, false);
}

//set the quality/size settings that NNO wants for map snaps
void OVERRIDE_LATELINK_gfxMapSnapPhotoScaleOptions(WorldRegion *region)
{
	g_fMapSnapMaximumPixelsPerWorldUnit = 1.3f;
	g_fMapSnapMinimumPixelsPerWorldUnit = 0.5f;
	g_fMapSnapMaximumSizeThreshold = 3100.0f;
	g_fMapSnapMinimumSizeThreshold = 100.0f;
}