#include "stdafx.h"
#pragma hdrstop

#pragma warning(disable:4995)
#ifndef _EDITOR
#include "../../xrEngine/render.h"
#endif
#pragma warning(default:4995)

#include <D3DX10Core.h>

#include "../xrRender/ResourceManager.h"
#include "../xrRender/tss.h"
#include "../xrRender/blenders/blender.h"
#include "../xrRender/blenders/blender_recorder.h"

#include "../xrRenderDX10/dx10BufferUtils.h"
#include "../xrRenderDX10/dx10ConstantBuffer.h"

#include <FlexibleVertexFormat.h>
using namespace FVF;

void fix_texture_name(LPSTR fn);

template <class T>
BOOL	reclaim		(xr_vector<T*>& vec, const T* ptr)
{
	xr_vector<T*>::iterator it	= vec.begin	();
	xr_vector<T*>::iterator end	= vec.end	();
	for (; it!=end; it++)
		if (*it == ptr)	{ vec.erase	(it); return TRUE; }
		return FALSE;
}

//--------------------------------------------------------------------------------------------------------------
class	includer				: public ID3DInclude
{
public:
	HRESULT __stdcall	Open	(D3D10_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
	{
		string_path				pname;
		strconcat				(sizeof(pname),pname,::Render->getShaderPath(),pFileName);
		IReader*		R		= FS.r_open	("$game_shaders$",pname);
		if (0==R)				{
			// possibly in shared directory or somewhere else - open directly
			R					= FS.r_open	("$game_shaders$",pFileName);
			if (0==R)			return			E_FAIL;
		}

		// duplicate and zero-terminate
		u32				size	= R->length();
		u8*				data	= xr_alloc<u8>	(size + 1);
		CopyMemory			(data,R->pointer(),size);
		data[size]				= 0;
		FS.r_close				(R);

		*ppData					= data;
		*pBytes					= size;
		return	D3D_OK;
	}
	HRESULT __stdcall	Close	(LPCVOID	pData)
	{
		xr_free	(pData);
		return	D3D_OK;
	}
};

//--------------------------------------------------------------------------------------------------------------
SState*		CResourceManager::_CreateState		(SimulatorStates& state_code)
{
	// Search equal state-code 
	for (u32 it=0; it<v_states.size(); it++)
	{
		SState*				C		= v_states[it];;
		SimulatorStates&	base	= C->state_code;
		if (base.equal(state_code))	return C;
	}

	// Create New
	v_states.push_back				(xr_new<SState>());
	v_states.back()->dwFlags		|= xr_resource_flagged::RF_REGISTERED;
#ifdef	USE_DX10
	v_states.back()->state			= ID3DState::Create(state_code);
#else	//	USE_DX10
	v_states.back()->state			= state_code.record();
#endif	//	USE_DX10
	v_states.back()->state_code		= state_code;
	return v_states.back();
}
void		CResourceManager::_DeleteState		(const SState* state)
{
	if (0==(state->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(v_states,state))						return;
	Msg	("! ERROR: Failed to find compiled stateblock");
}

//--------------------------------------------------------------------------------------------------------------
SPass*		CResourceManager::_CreatePass			(ref_state& _state, ref_ps& _ps, ref_vs& _vs, ref_gs& _gs, ref_ctable& _ctable, ref_texture_list& _T, ref_matrix_list& _M, ref_constant_list& _C)
{
	for (u32 it=0; it<v_passes.size(); it++)
		if (v_passes[it]->equal(_state,_ps,_vs,_gs,_ctable,_T,_M,_C))
			return v_passes[it];

	SPass*	P					=	xr_new<SPass>();
	P->dwFlags					|=	xr_resource_flagged::RF_REGISTERED;
	P->state					=	_state;
	P->ps						=	_ps;
	P->vs						=	_vs;
	P->gs						=	_gs;
	P->constants				=	_ctable;
	P->T						=	_T;
#ifdef _EDITOR
	P->M						=	_M;
#endif
	P->C						=	_C;

	v_passes.push_back			(P);
	return v_passes.back();
}

void		CResourceManager::_DeletePass			(const SPass* P)
{
	if (0==(P->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(v_passes,P))						return;
	Msg	("! ERROR: Failed to find compiled pass");
}

//--------------------------------------------------------------------------------------------------------------
SVS*	CResourceManager::_CreateVS		(LPCSTR _name)
{
	string_path			name;
	xr_strcpy				(name,_name);
	if (0 == ::Render->m_skinning)	strcat(name,"_0");
	if (1 == ::Render->m_skinning)	strcat(name,"_1");
	if (2 == ::Render->m_skinning)	strcat(name,"_2");
	if (3 == ::Render->m_skinning)	strcat(name,"_3");
	if (4 == ::Render->m_skinning)	strcat(name,"_4");
	LPSTR N				= LPSTR		(name);
	map_VS::iterator I	= m_vs.find	(N);
	if (I!=m_vs.end())	return I->second;
	else
	{
		SVS*	_vs					= xr_new<SVS>	();
		_vs->dwFlags				|= xr_resource_flagged::RF_REGISTERED;
		m_vs.insert					(std::make_pair(_vs->set_name(name),_vs));
		//_vs->vs				= NULL;
		//_vs->signature		= NULL;
		if (0==_stricmp(_name,"null"))	{
			return _vs;
		}

		includer					Includer;
		ID3DBlob*					pShaderBuf	= NULL;
		ID3DBlob*					pErrorBuf	= NULL;
		//LPD3DXSHADER_CONSTANTTABLE	pConstants	= NULL;
		
		string_path					cname;
		strconcat					(sizeof(cname),cname,::Render->getShaderPath(),_name,".vs");
		FS.update_path				(cname,	"$game_shaders$", cname);
		//		LPCSTR						target		= NULL;

		IReader*					fs			= FS.r_open(cname);
		//	TODO: DX10: HACK: Implement all shaders. Remove this for PS
		if (!fs)
		{
			string1024			tmp;
			sprintf				(tmp, "DX10: %s is missing. Replace with stub_default.vs", cname);
			Msg					(tmp);
			strconcat					(sizeof(cname), cname,::Render->getShaderPath(),"stub_default",".vs");
			FS.update_path				(cname,	"$game_shaders$", cname);
			fs		= FS.r_open(cname);
		}
		R_ASSERT3					(fs, "shader file doesnt exist", cname);

		// Select target
		LPCSTR						c_target	= "vs_4_0";
		LPCSTR						c_entry		= "main";
		LPSTR pfs					= xr_alloc<char>(fs->length() + 1);
		strncpy						(pfs, (LPCSTR)fs->pointer(), fs->length());
		pfs							[fs->length()] = 0;
		xr_free(pfs);

		// vertex
		R_ASSERT2					(fs,cname);
		//hr = ::Render->shader_compile(name,LPCSTR(fs->pointer()),fs->length(), NULL, &Includer, c_entry, c_target, D3DXSHADER_DEBUG | D3DXSHADER_PACKMATRIX_ROWMAJOR /*| D3DXSHADER_PREFER_FLOW_CONTROL*/, &pShaderBuf, &pErrorBuf, NULL);
//		hr = ::Render->shader_compile(name,LPCSTR(fs->pointer()),fs->length(), NULL, &Includer, c_entry, c_target, D3D10_SHADER_DEBUG | D3D10_SHADER_PACK_MATRIX_ROW_MAJOR /*| D3DXSHADER_PREFER_FLOW_CONTROL*/, &pShaderBuf, &pErrorBuf, NULL);
		auto hr = ::Render->shader_compile(name,LPCSTR(fs->pointer()),fs->length(), NULL, &Includer, c_entry, c_target, D3D10_SHADER_PACK_MATRIX_ROW_MAJOR /*| D3DXSHADER_PREFER_FLOW_CONTROL*/, &pShaderBuf, &pErrorBuf, NULL);
		//hr = ::Render->shader_compile(name,LPCSTR(fs->pointer()),fs->length(), NULL, &Includer, c_entry, c_target, D3D10_SHADER_PACK_MATRIX_ROW_MAJOR | D3D10_SHADER_AVOID_FLOW_CONTROL /*| D3DXSHADER_PREFER_FLOW_CONTROL*/, &pShaderBuf, &pErrorBuf, NULL);
		FS.r_close					(fs);

		if (SUCCEEDED(hr))
		{
			if (pShaderBuf)
			{
				hr = HW.pDevice->CreateVertexShader(pShaderBuf->GetBufferPointer(), pShaderBuf->GetBufferSize(), &_vs->vs);
				if (SUCCEEDED(hr))	
				{
					ID3D10ShaderReflection *pReflection = 0;

					hr = D3D10ReflectShader( pShaderBuf->GetBufferPointer(), pShaderBuf->GetBufferSize(), &pReflection);
					
					//	Parse constant, texture, sampler binding
					//	Store input signature blob
					if (SUCCEEDED(hr) && pReflection)
					{
						//	TODO: DX10: share the same input signatures

						//	Store input signature (need only for VS)
						//CHK_DX( D3D10GetInputSignatureBlob(pShaderBuf->GetBufferPointer(), pShaderBuf->GetBufferSize(), &_vs->signature) );
						ID3DBlob*	pSignatureBlob;
						CHK_DX( D3D10GetInputSignatureBlob(pShaderBuf->GetBufferPointer(), pShaderBuf->GetBufferSize(), &pSignatureBlob) );
						VERIFY(pSignatureBlob);

						_vs->signature = _CreateInputSignature(pSignatureBlob);

						_RELEASE(pSignatureBlob);

						//	Let constant table parse it's data
						_vs->constants.parse(pReflection,RC_dest_vertex);

						_RELEASE(pReflection);
					}
				}
				_RELEASE(pShaderBuf);
			}
			else
				hr = E_FAIL;
		} 
		else 
		{
			VERIFY	(pErrorBuf);
			Log		("! VS: ", _name);
			Log		("! error: ",(LPCSTR)pErrorBuf->GetBufferPointer());
		}
		_RELEASE	(pShaderBuf);
		_RELEASE	(pErrorBuf);
//		pConstants	= NULL;
		R_CHK(hr);
		return		_vs;
	}
}

void	CResourceManager::_DeleteVS			(const SVS* vs)
{
	if (0==(vs->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	LPSTR N				= LPSTR		(*vs->cName);
	map_VS::iterator I	= m_vs.find	(N);
	if (I!=m_vs.end())	
	{
		m_vs.erase(I);
		xr_vector<SDeclaration*>::iterator iDecl;
		for (iDecl = v_declarations.begin(); iDecl!=v_declarations.end(); ++iDecl)
		{
			xr_map<ID3DBlob*, ID3D10InputLayout*>::iterator iLayout;
			iLayout = (*iDecl)->vs_to_layout.find(vs->signature->signature);
			if (iLayout!=(*iDecl)->vs_to_layout.end())
			{
				//	Release vertex layout
				_RELEASE(iLayout->second);
				(*iDecl)->vs_to_layout.erase(iLayout);
			}
		}
		return;
	}
	Msg	("! ERROR: Failed to find compiled vertex-shader '%s'",*vs->cName);
}

//--------------------------------------------------------------------------------------------------------------
SPS*	CResourceManager::_CreatePS			(LPCSTR _name)
{
	string_path			name;
	xr_strcpy				(name,_name);
	if (0 == ::Render->m_MSAASample)	strcat(name,"_0");
	if (1 == ::Render->m_MSAASample)	strcat(name,"_1");
	if (2 == ::Render->m_MSAASample)	strcat(name,"_2");
	if (3 == ::Render->m_MSAASample)	strcat(name,"_3");
	if (4 == ::Render->m_MSAASample)	strcat(name,"_4");
	if (5 == ::Render->m_MSAASample)	strcat(name,"_5");
	if (6 == ::Render->m_MSAASample)	strcat(name,"_6");
	if (7 == ::Render->m_MSAASample)	strcat(name,"_7");
	LPSTR N				= LPSTR(name);
	map_PS::iterator I	= m_ps.find	(N);
	if (I!=m_ps.end())	return		I->second;
	else
	{
		SPS*	_ps					=	xr_new<SPS>	();
		_ps->dwFlags				|=	xr_resource_flagged::RF_REGISTERED;
		m_ps.insert					(std::make_pair(_ps->set_name(name),_ps));
		if (0==_stricmp(_name,"null"))	{
			_ps->ps				= NULL;
			return _ps;
		}

		// Open file
		includer					Includer;
		string_path					cname;
		strconcat					(sizeof(cname), cname,::Render->getShaderPath(),_name,".ps");
		FS.update_path				(cname,	"$game_shaders$", cname);

		// duplicate and zero-terminate
		IReader*		R		= FS.r_open(cname);
		//	TODO: DX10: HACK: Implement all shaders. Remove this for PS
		if (!R)
		{
			string1024			tmp;
			//	TODO: HACK: Test failure
			//Memory.mem_compact();
			sprintf				(tmp, "DX10: %s is missing. Replace with stub_default.ps", cname);
			Msg					(tmp);
			strconcat					(sizeof(cname), cname,::Render->getShaderPath(),"stub_default",".ps");
			FS.update_path				(cname,	"$game_shaders$", cname);
			R		= FS.r_open(cname);
		}
		R_ASSERT2				(R,cname);
		u32				size	= R->length();
		char*			data	= xr_alloc<char>(size + 1);
		CopyMemory			(data,R->pointer(),size);
		data[size]				= 0;
		FS.r_close				(R);

		// Select target
		LPCSTR						c_target	= "ps_4_0";
		LPCSTR						c_entry		= "main";

		// Compile
		ID3DBlob*					pShaderBuf	= NULL;
		ID3DBlob*					pErrorBuf	= NULL;
		//LPD3DXSHADER_CONSTANTTABLE	pConstants	= NULL;
		HRESULT						_hr			= S_OK;
		//_hr = ::Render->shader_compile	(name,data,size, NULL, &Includer, c_entry, c_target, D3DXSHADER_DEBUG | D3DXSHADER_PACKMATRIX_ROWMAJOR, &pShaderBuf, &pErrorBuf, NULL);
		//_hr = ::Render->shader_compile(name,data,size, NULL, &Includer, c_entry, c_target, D3D10_SHADER_DEBUG | D3D10_SHADER_PACK_MATRIX_ROW_MAJOR, &pShaderBuf, &pErrorBuf, NULL);
		_hr = ::Render->shader_compile(name,data,size, NULL, &Includer, c_entry, c_target, D3D10_SHADER_PACK_MATRIX_ROW_MAJOR, &pShaderBuf, &pErrorBuf, NULL);
		//_hr = ::Render->shader_compile(name,data,size, NULL, &Includer, c_entry, c_target, D3D10_SHADER_PACK_MATRIX_ROW_MAJOR | D3D10_SHADER_AVOID_FLOW_CONTROL, &pShaderBuf, &pErrorBuf, NULL);
		//_hr = D3DXCompileShader		(text,text_size, NULL, &Includer, c_entry, c_target, D3DXSHADER_DEBUG | D3DXSHADER_PACKMATRIX_ROWMAJOR, &pShaderBuf, &pErrorBuf, NULL);
		xr_free						(data);

		if (SUCCEEDED(_hr))
		{
			if (pShaderBuf)
			{
				_hr = HW.pDevice->CreatePixelShader	(pShaderBuf->GetBufferPointer(), pShaderBuf->GetBufferSize(), &_ps->ps);

				ID3D10ShaderReflection *pReflection = 0;

				_hr = D3D10ReflectShader( pShaderBuf->GetBufferPointer(), pShaderBuf->GetBufferSize(), &pReflection);

				//	Parse constant, texture, sampler binding
				//	Store input signature blob
				if (SUCCEEDED(_hr) && pReflection)
				{
					//	Let constant table parse it's data
					_ps->constants.parse(pReflection,RC_dest_pixel);

					_RELEASE(pReflection);
				}
				/*
				if (SUCCEEDED(_hr))	{
					LPCVOID			data		= NULL;
					_hr	= D3DXFindShaderComment	((DWORD*)pShaderBuf->GetBufferPointer(),MAKEFOURCC('C','T','A','B'),&data,NULL);
					if (SUCCEEDED(_hr) && data)
					{
						pConstants				= LPD3DXSHADER_CONSTANTTABLE(data);
						_ps->constants.parse	(pConstants,0x1);
					} else	_hr = E_FAIL;
				}
				*/
				_RELEASE(pShaderBuf);
			}
			else	_hr = E_FAIL;
		}else
		{
			VERIFY	(pErrorBuf);
			Log		("! PS: ", _name);
			Msg("error is %s", (LPCSTR)pErrorBuf->GetBufferPointer());
		}
		_RELEASE		(pShaderBuf);
		_RELEASE		(pErrorBuf);

		if (FAILED(_hr))
			Msg			("Can't compile shader %s",_name);

		CHECK_OR_EXIT		(
			!FAILED(_hr),
			make_string("Your video card doesn't meet game requirements\n\nPixel Shaders v1.1 or higher required")
			);
		return			_ps;
	}
}
void	CResourceManager::_DeletePS			(const SPS* ps)
{
	if (0==(ps->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	LPSTR N				= LPSTR		(*ps->cName);
	map_PS::iterator I	= m_ps.find	(N);
	if (I!=m_ps.end())	{
		m_ps.erase(I);
		return;
	}
	Msg	("! ERROR: Failed to find compiled pixel-shader '%s'",*ps->cName);
}

//--------------------------------------------------------------------------------------------------------------
SGS*	CResourceManager::_CreateGS			(LPCSTR name)
{
	LPSTR N				= LPSTR(name);
	map_GS::iterator I	= m_gs.find	(N);
	if (I!=m_gs.end())	return		I->second;
	else
	{
		SGS*	_gs					=	xr_new<SGS>	();
		_gs->dwFlags				|=	xr_resource_flagged::RF_REGISTERED;
		m_gs.insert					(std::make_pair(_gs->set_name(name),_gs));
		if (0==_stricmp(name,"null"))	{
			_gs->gs				= NULL;
			return _gs;
		}

		// Open file
		includer					Includer;
		string_path					cname;
		strconcat					(sizeof(cname), cname,::Render->getShaderPath(),name,".gs");
		FS.update_path				(cname,	"$game_shaders$", cname);

		// duplicate and zero-terminate
		IReader*		R		= FS.r_open(cname);
		//	TODO: DX10: HACK: Implement all shaders. Remove this for PS
		if (!R)
		{
			string1024			tmp;
			//	TODO: HACK: Test failure
			//Memory.mem_compact();
			sprintf				(tmp, "DX10: %s is missing. Replace with stub_default.gs", cname);
			Msg					(tmp);
			strconcat					(sizeof(cname), cname,::Render->getShaderPath(),"stub_default",".gs");
			FS.update_path				(cname,	"$game_shaders$", cname);
			R		= FS.r_open(cname);
		}
		R_ASSERT2				(R,cname);
		u32				size	= R->length();
		char*			data	= xr_alloc<char>(size + 1);
		CopyMemory			(data,R->pointer(),size);
		data[size]				= 0;
		FS.r_close				(R);

		// Select target
		LPCSTR						c_target	= "gs_4_0";
		LPCSTR						c_entry		= "main";

		// Compile
		ID3DBlob*					pShaderBuf	= NULL;
		ID3DBlob*					pErrorBuf	= NULL;
		HRESULT						_hr			= S_OK;
		_hr = ::Render->shader_compile(name,data,size, NULL, &Includer, c_entry, c_target, D3D10_SHADER_PACK_MATRIX_ROW_MAJOR, &pShaderBuf, &pErrorBuf, NULL);
		//_hr = ::Render->shader_compile(name,data,size, NULL, &Includer, c_entry, c_target, D3D10_SHADER_PACK_MATRIX_ROW_MAJOR | D3D10_SHADER_AVOID_FLOW_CONTROL, &pShaderBuf, &pErrorBuf, NULL);
		xr_free						(data);

		if (SUCCEEDED(_hr))
		{
			if (pShaderBuf)
			{
				_hr = HW.pDevice->CreateGeometryShader	(pShaderBuf->GetBufferPointer(), pShaderBuf->GetBufferSize(), &_gs->gs);

				ID3D10ShaderReflection *pReflection = 0;

				_hr = D3D10ReflectShader( pShaderBuf->GetBufferPointer(), pShaderBuf->GetBufferSize(), &pReflection);

				//	Parse constant, texture, sampler binding
				//	Store input signature blob
				if (SUCCEEDED(_hr) && pReflection)
				{
					//	Let constant table parse it's data
					_gs->constants.parse(pReflection,RC_dest_geometry);

					_RELEASE(pReflection);
				}

				_RELEASE(pShaderBuf);
			}
			else	_hr = E_FAIL;
		}else
		{
			Msg("error is %s", (LPCSTR)pErrorBuf->GetBufferPointer());
		}
		_RELEASE		(pShaderBuf);
		_RELEASE		(pErrorBuf);

		if (FAILED(_hr))
			Msg			("Can't compile shader %s",name);

		CHECK_OR_EXIT		(
			!FAILED(_hr),
			make_string("Your video card doesn't meet game requirements\n\nPixel Shaders v1.1 or higher required")
			);
		return			_gs;
	}
}
void	CResourceManager::_DeleteGS			(const SGS* gs)
{
	if (0==(gs->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	LPSTR N				= LPSTR		(*gs->cName);
	map_GS::iterator I	= m_gs.find	(N);
	if (I!=m_gs.end())	{
		m_gs.erase(I);
		return;
	}
	Msg	("! ERROR: Failed to find compiled geometry shader '%s'",*gs->cName);
}

//--------------------------------------------------------------------------------------------------------------
static BOOL	dcl_equal			(D3DVERTEXELEMENT9* a, D3DVERTEXELEMENT9* b)
{
	// check sizes
	u32 a_size = GetDeclLength(a);
	u32 b_size = GetDeclLength(b);
	if (a_size!=b_size)	return FALSE;
	return 0==memcmp	(a,b,a_size*sizeof(D3DVERTEXELEMENT9));
}

SDeclaration*	CResourceManager::_CreateDecl	(D3DVERTEXELEMENT9* dcl)
{
	// Search equal code
	for (u32 it=0; it<v_declarations.size(); it++)
	{
		SDeclaration*		D		= v_declarations[it];;
		if (dcl_equal(dcl,&*D->dcl_code.begin()))	return D;
	}

	// Create _new
	SDeclaration* D			= xr_new<SDeclaration>();
	u32 dcl_size = GetDeclLength(dcl) + 1;
	//	Don't need it for DirectX 10 here
	//CHK_DX					(HW.pDevice->CreateVertexDeclaration(dcl,&D->dcl));
	D->dcl_code.assign		(dcl,dcl+dcl_size);
	dx10BufferUtils::ConvertVertexDeclaration(D->dcl_code, D->dx10_dcl_code);
	D->dwFlags				|= xr_resource_flagged::RF_REGISTERED;
	v_declarations.push_back(D);
	return D;
}

void		CResourceManager::_DeleteDecl		(const SDeclaration* dcl)
{
	if (0==(dcl->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(v_declarations,dcl))					return;
	Msg	("! ERROR: Failed to find compiled vertex-declarator");
}

//--------------------------------------------------------------------------------------------------------------
R_constant_table*	CResourceManager::_CreateConstantTable	(R_constant_table& C)
{
	if (C.empty())		return NULL;

	for (u32 it=0; it<v_constant_tables.size(); it++)
		if (v_constant_tables[it]->equal(C))	return v_constant_tables[it];
	v_constant_tables.push_back			(xr_new<R_constant_table>(C));
	v_constant_tables.back()->dwFlags	|=	xr_resource_flagged::RF_REGISTERED;
	return v_constant_tables.back		();
}
void				CResourceManager::_DeleteConstantTable	(const R_constant_table* C)
{
	if (0==(C->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(v_constant_tables,C))				return;
	Msg	("! ERROR: Failed to find compiled constant-table");
}

//--------------------------------------------------------------------------------------------------------------
CRT*	CResourceManager::_CreateRT		(LPCSTR Name, u32 w, u32 h,	D3DFORMAT f, u32 SampleCount )
{
	R_ASSERT(Name && Name[0] && w && h);

	// ***** first pass - search already created RT
	LPSTR N = LPSTR(Name);
	map_RT::iterator I = m_rtargets.find	(N);
	if (I!=m_rtargets.end())	return		I->second;
	else
	{
		CRT *RT					=	xr_new<CRT>();
		RT->dwFlags				|=	xr_resource_flagged::RF_REGISTERED;
		m_rtargets.insert		(std::make_pair(RT->set_name(Name),RT));
		if (Device.b_is_Ready)	RT->create	(Name,w,h,f, SampleCount );
		return					RT;
	}
}
void	CResourceManager::_DeleteRT		(const CRT* RT)
{
	if (0==(RT->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	LPSTR N				= LPSTR		(*RT->cName);
	map_RT::iterator I	= m_rtargets.find	(N);
	if (I!=m_rtargets.end())	{
		m_rtargets.erase(I);
		return;
	}
	Msg	("! ERROR: Failed to find render-target '%s'",*RT->cName);
}
/*	//	DX10 cut 
//--------------------------------------------------------------------------------------------------------------
CRTC*	CResourceManager::_CreateRTC		(LPCSTR Name, u32 size,	D3DFORMAT f)
{
	R_ASSERT(Name && Name[0] && size);

	// ***** first pass - search already created RTC
	LPSTR N = LPSTR(Name);
	map_RTC::iterator I = m_rtargets_c.find	(N);
	if (I!=m_rtargets_c.end())	return I->second;
	else
	{
		CRTC *RT				=	xr_new<CRTC>();
		RT->dwFlags				|=	xr_resource_flagged::RF_REGISTERED;
		m_rtargets_c.insert		(std::make_pair(RT->set_name(Name),RT));
		if (Device.b_is_Ready)	RT->create	(Name,size,f);
		return					RT;
	}
}
void	CResourceManager::_DeleteRTC		(const CRTC* RT)
{
	if (0==(RT->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	LPSTR N				= LPSTR		(*RT->cName);
	map_RTC::iterator I	= m_rtargets_c.find	(N);
	if (I!=m_rtargets_c.end())	{
		m_rtargets_c.erase(I);
		return;
	}
	Msg	("! ERROR: Failed to find render-target '%s'",*RT->cName);
}
*/
//--------------------------------------------------------------------------------------------------------------
void	CResourceManager::DBG_VerifyGeoms	()
{
	/*
	for (u32 it=0; it<v_geoms.size(); it++)
	{
	SGeometry* G					= v_geoms[it];

	D3DVERTEXELEMENT9		test	[MAXD3DDECLLENGTH + 1];
	u32						size	= 0;
	G->dcl->GetDeclaration			(test,(unsigned int*)&size);
	u32 vb_stride = ComputeVertexSize(test, 0);
	u32 vb_stride_cached			= G->vb_stride;
	R_ASSERT						(vb_stride == vb_stride_cached);
	}
	*/
}

SGeometry*	CResourceManager::CreateGeom	(D3DVERTEXELEMENT9* decl, ID3DVertexBuffer* vb, ID3DIndexBuffer* ib)
{
	R_ASSERT			(decl && vb);

	SDeclaration* dcl	= _CreateDecl			(decl);
	u32 vb_stride = ComputeVertexSize(decl, 0);

	// ***** first pass - search already loaded shader
	for (u32 it=0; it<v_geoms.size(); it++)
	{
		SGeometry& G	= *(v_geoms[it]);
		if ((G.dcl==dcl) && (G.vb==vb) && (G.ib==ib) && (G.vb_stride==vb_stride))	return v_geoms[it];
	}

	SGeometry *Geom		=	xr_new<SGeometry>	();
	Geom->dwFlags		|=	xr_resource_flagged::RF_REGISTERED;
	Geom->dcl			=	dcl;
	Geom->vb			=	vb;
	Geom->vb_stride		=	vb_stride;
	Geom->ib			=	ib;
	v_geoms.push_back	(Geom);
	return	Geom;
}
SGeometry*	CResourceManager::CreateGeom		(u32 FVF, ID3DVertexBuffer* vb, ID3DIndexBuffer* ib)
{
	auto dcl = std::vector<D3DVERTEXELEMENT9>(MAXD3DDECLLENGTH + 1);
	CHK_DX(CreateDeclFromFVF(FVF, dcl));
	SGeometry* g = CreateGeom(dcl.data(), vb, ib);

	return	g;
}

void		CResourceManager::DeleteGeom		(const SGeometry* Geom)
{
	if (0==(Geom->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(v_geoms,Geom))							return;
	Msg	("! ERROR: Failed to find compiled geometry-declaration");
}

//--------------------------------------------------------------------------------------------------------------
CTexture* CResourceManager::_CreateTexture	(LPCSTR _Name)
{
	// DBG_VerifyTextures	();
	if (0==xr_strcmp(_Name,"null"))	return 0;
	R_ASSERT		(_Name && _Name[0]);
	string_path		Name;
	xr_strcpy			(Name,_Name); //. andy if (strext(Name)) *strext(Name)=0;
	fix_texture_name (Name);
	// ***** first pass - search already loaded texture
	LPSTR N			= LPSTR(Name);
	map_TextureIt I = m_textures.find	(N);
	if (I!=m_textures.end())	return	I->second;
	else
	{
		CTexture *	T		=	xr_new<CTexture>();
		T->dwFlags			|=	xr_resource_flagged::RF_REGISTERED;
		m_textures.insert	(std::make_pair(T->set_name(Name),T));
		T->Preload			();
		if (Device.b_is_Ready && !bDeferredLoad) T->Load();
		return		T;
	}
}
void	CResourceManager::_DeleteTexture		(const CTexture* T)
{
	// DBG_VerifyTextures	();

	if (0==(T->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	LPSTR N					= LPSTR		(*T->cName);
	map_Texture::iterator I	= m_textures.find	(N);
	if (I!=m_textures.end())	{
		m_textures.erase(I);
		return;
	}
	Msg	("! ERROR: Failed to find texture surface '%s'",*T->cName);
}

#ifdef DEBUG
void	CResourceManager::DBG_VerifyTextures	()
{
	map_Texture::iterator I		= m_textures.begin	();
	map_Texture::iterator E		= m_textures.end	();
	for (; I!=E; I++) 
	{
		R_ASSERT(I->first);
		R_ASSERT(I->second);
		R_ASSERT(I->second->cName);
		R_ASSERT(0==xr_strcmp(I->first,*I->second->cName));
	}
}
#endif

//--------------------------------------------------------------------------------------------------------------
CMatrix*	CResourceManager::_CreateMatrix	(LPCSTR Name)
{
	R_ASSERT(Name && Name[0]);
	if (0==_stricmp(Name,"$null"))	return NULL;

	LPSTR N = LPSTR(Name);
	map_Matrix::iterator I = m_matrices.find	(N);
	if (I!=m_matrices.end())	return I->second;
	else
	{
		CMatrix* M			=	xr_new<CMatrix>();
		M->dwFlags			|=	xr_resource_flagged::RF_REGISTERED;
		M->dwReference		=	1;
		m_matrices.insert	(std::make_pair(M->set_name(Name),M));
		return			M;
	}
}
void	CResourceManager::_DeleteMatrix		(const CMatrix* M)
{
	if (0==(M->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	LPSTR N					= LPSTR		(*M->cName);
	map_Matrix::iterator I	= m_matrices.find	(N);
	if (I!=m_matrices.end())	{
		m_matrices.erase(I);
		return;
	}
	Msg	("! ERROR: Failed to find xform-def '%s'",*M->cName);
}
void	CResourceManager::ED_UpdateMatrix		(LPCSTR Name, CMatrix* data)
{
	CMatrix*	M	= _CreateMatrix	(Name);
	*M				= *data;
}
//--------------------------------------------------------------------------------------------------------------
CConstant*	CResourceManager::_CreateConstant	(LPCSTR Name)
{
	R_ASSERT(Name && Name[0]);
	if (0==_stricmp(Name,"$null"))	return NULL;

	LPSTR N = LPSTR(Name);
	map_Constant::iterator I	= m_constants.find	(N);
	if (I!=m_constants.end())	return I->second;
	else
	{
		CConstant* C		=	xr_new<CConstant>();
		C->dwFlags			|=	xr_resource_flagged::RF_REGISTERED;
		C->dwReference		=	1;
		m_constants.insert	(std::make_pair(C->set_name(Name),C));
		return	C;
	}
}
void	CResourceManager::_DeleteConstant		(const CConstant* C)
{
	if (0==(C->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	LPSTR N				= LPSTR				(*C->cName);
	map_Constant::iterator I	= m_constants.find	(N);
	if (I!=m_constants.end())	{
		m_constants.erase(I);
		return;
	}
	Msg	("! ERROR: Failed to find R1-constant-def '%s'",*C->cName);
}

void	CResourceManager::ED_UpdateConstant	(LPCSTR Name, CConstant* data)
{
	CConstant*	C	= _CreateConstant	(Name);
	*C				= *data;
}

//--------------------------------------------------------------------------------------------------------------
bool	cmp_tl	(const std::pair<u32,ref_texture>& _1, const std::pair<u32,ref_texture>& _2)	{
	return _1.first < _2.first;
}
STextureList*	CResourceManager::_CreateTextureList(STextureList& L)
{
	std::sort	(L.begin(),L.end(),cmp_tl);
	for (u32 it=0; it<lst_textures.size(); it++)
	{
		STextureList*	base		= lst_textures[it];
		if (L.equal(*base))			return base;
	}
	STextureList*	lst		=	xr_new<STextureList>(L);
	lst->dwFlags			|=	xr_resource_flagged::RF_REGISTERED;
	lst_textures.push_back	(lst);
	return lst;
}
void			CResourceManager::_DeleteTextureList(const STextureList* L)
{
	if (0==(L->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(lst_textures,L))					return;
	Msg	("! ERROR: Failed to find compiled list of textures");
}
//--------------------------------------------------------------------------------------------------------------
SMatrixList*	CResourceManager::_CreateMatrixList(SMatrixList& L)
{
	BOOL bEmpty = TRUE;
	for (u32 i=0; i<L.size(); i++)	if (L[i]) { bEmpty=FALSE; break; }
	if (bEmpty)	return NULL;

	for (u32 it=0; it<lst_matrices.size(); it++)
	{
		SMatrixList*	base		= lst_matrices[it];
		if (L.equal(*base))			return base;
	}
	SMatrixList*	lst		=	xr_new<SMatrixList>(L);
	lst->dwFlags			|=	xr_resource_flagged::RF_REGISTERED;
	lst_matrices.push_back	(lst);
	return lst;
}
void			CResourceManager::_DeleteMatrixList ( const SMatrixList* L )
{
	if (0==(L->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(lst_matrices,L))					return;
	Msg	("! ERROR: Failed to find compiled list of xform-defs");
}
//--------------------------------------------------------------------------------------------------------------
SConstantList*	CResourceManager::_CreateConstantList(SConstantList& L)
{
	BOOL bEmpty = TRUE;
	for (u32 i=0; i<L.size(); i++)	if (L[i]) { bEmpty=FALSE; break; }
	if (bEmpty)	return NULL;

	for (u32 it=0; it<lst_constants.size(); it++)
	{
		SConstantList*	base		= lst_constants[it];
		if (L.equal(*base))			return base;
	}
	SConstantList*	lst		=	xr_new<SConstantList>(L);
	lst->dwFlags			|=	xr_resource_flagged::RF_REGISTERED;
	lst_constants.push_back	(lst);
	return lst;
}
void			CResourceManager::_DeleteConstantList(const SConstantList* L )
{
	if (0==(L->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(lst_constants,L))					return;
	Msg	("! ERROR: Failed to find compiled list of r1-constant-defs");
}
//--------------------------------------------------------------------------------------------------------------
dx10ConstantBuffer* CResourceManager::_CreateConstantBuffer(ID3D10ShaderReflectionConstantBuffer* pTable)
{
	VERIFY(pTable);
	dx10ConstantBuffer	*pTempBuffer = xr_new<dx10ConstantBuffer>(pTable);

	for (u32 it=0; it<v_constant_buffer.size(); it++)
	{
		dx10ConstantBuffer*	buf		= v_constant_buffer[it];
		if (pTempBuffer->Similar(*buf))			
		{
			xr_delete(pTempBuffer);
			return buf;
		}
	}

	pTempBuffer->dwFlags |= xr_resource_flagged::RF_REGISTERED;
	v_constant_buffer.push_back	(pTempBuffer);
	return pTempBuffer;
}
//--------------------------------------------------------------------------------------------------------------
void CResourceManager::_DeleteConstantBuffer(const dx10ConstantBuffer* pBuffer)
{
	if (0==(pBuffer->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(v_constant_buffer,pBuffer))						return;
	Msg	("! ERROR: Failed to find compiled constant buffer");
}

//--------------------------------------------------------------------------------------------------------------
SInputSignature* CResourceManager::_CreateInputSignature(ID3DBlob* pBlob)
{
	VERIFY(pBlob);

	for (u32 it=0; it<v_input_signature.size(); it++)
	{
		SInputSignature*	sign		= v_input_signature[it];
		if ( (pBlob->GetBufferSize() == sign->signature->GetBufferSize()) &&
			(!(memcmp(pBlob->GetBufferPointer(), sign->signature->GetBufferPointer(), pBlob->GetBufferSize()))))
		{
			return sign;
		}
	}

	SInputSignature	*pSign = xr_new<SInputSignature>(pBlob);

	pSign->dwFlags |= xr_resource_flagged::RF_REGISTERED;
	v_input_signature.push_back	(pSign);

	return pSign;
}
//--------------------------------------------------------------------------------------------------------------
void CResourceManager::_DeleteInputSignature(const SInputSignature* pSignature)
{
	if (0==(pSignature->dwFlags&xr_resource_flagged::RF_REGISTERED))	return;
	if (reclaim(v_input_signature, pSignature))						return;
	Msg	("! ERROR: Failed to find compiled constant buffer");
}