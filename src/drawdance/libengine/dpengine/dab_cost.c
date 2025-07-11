// SPDX-License-Identifier: GPL-3.0-or-later
// Auto-generated by generate_dab_cost.py
#include "dab_cost.h"
#include <dpcommon/common.h>
#include <dpmsg/blend_mode.h>

double DP_dab_cost_pixel(bool indirect, int blend_mode)
{
    if (indirect) {
        return 13.481322283737024;
    }
    else {
        switch (blend_mode) {
        case DP_BLEND_MODE_ERASE:
        case DP_BLEND_MODE_ERASE_PRESERVE:
            return 4.769305351787774;
        case DP_BLEND_MODE_NORMAL:
            return 1.2311402383698578;
        case DP_BLEND_MODE_MULTIPLY:
            return 5.989750026912725;
        case DP_BLEND_MODE_DIVIDE:
            return 28.254504752018452;
        case DP_BLEND_MODE_BURN:
            return 28.753766712802765;
        case DP_BLEND_MODE_DODGE:
            return 27.96435163398693;
        case DP_BLEND_MODE_DARKEN:
            return 6.297176824298347;
        case DP_BLEND_MODE_LIGHTEN:
            return 6.004300161476356;
        case DP_BLEND_MODE_SUBTRACT:
            return 6.418920899653979;
        case DP_BLEND_MODE_ADD:
            return 6.455187912341407;
        case DP_BLEND_MODE_RECOLOR:
            return 1.2902103191080354;
        case DP_BLEND_MODE_BEHIND:
        case DP_BLEND_MODE_BEHIND_PRESERVE:
            return 2.046001414840446;
        case DP_BLEND_MODE_COLOR_ERASE:
        case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
            return 14.568697101114957;
        case DP_BLEND_MODE_SCREEN:
            return 7.095914963475586;
        case DP_BLEND_MODE_NORMAL_AND_ERASER:
            return 1.3343192310649752;
        case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
            return 4.3291261514801995;
        case DP_BLEND_MODE_OVERLAY:
            return 7.629804382929643;
        case DP_BLEND_MODE_HARD_LIGHT:
            return 6.923692179930796;
        case DP_BLEND_MODE_SOFT_LIGHT:
            return 8.327986605151866;
        case DP_BLEND_MODE_LINEAR_BURN:
            return 6.469190049980776;
        case DP_BLEND_MODE_LINEAR_LIGHT:
            return 7.027186712802768;
        case DP_BLEND_MODE_HUE:
            return 34.28230883506344;
        case DP_BLEND_MODE_SATURATION:
            return 23.751073571703188;
        case DP_BLEND_MODE_LUMINOSITY:
            return 15.915821276432142;
        case DP_BLEND_MODE_COLOR:
            return 41.96999028066128;
        case DP_BLEND_MODE_COMPARE_DENSITY_SOFT:
            return 0.9710173779315647;
        case DP_BLEND_MODE_COMPARE_DENSITY:
            return 1.0903419454056131;
        case DP_BLEND_MODE_VIVID_LIGHT:
            return 28.963694994232988;
        case DP_BLEND_MODE_PIN_LIGHT:
            return 7.712222998846598;
        case DP_BLEND_MODE_DIFFERENCE:
            return 6.767272702806612;
        case DP_BLEND_MODE_DARKER_COLOR:
            return 12.132091757016532;
        case DP_BLEND_MODE_LIGHTER_COLOR:
            return 12.195546689734718;
        case DP_BLEND_MODE_SHADE_SAI:
            return 5.314832656670511;
        case DP_BLEND_MODE_SHADE_SHINE_SAI:
            return 5.23832585159554;
        case DP_BLEND_MODE_BURN_SAI:
            return 22.548101391772395;
        case DP_BLEND_MODE_DODGE_SAI:
            return 20.012023137254904;
        case DP_BLEND_MODE_BURN_DODGE_SAI:
            return 22.583226643598614;
        case DP_BLEND_MODE_HARD_MIX_SAI:
            return 5.54010739715494;
        case DP_BLEND_MODE_DIFFERENCE_SAI:
            return 4.533281261053441;
        case DP_BLEND_MODE_MARKER:
            return 13.28549258746636;
        case DP_BLEND_MODE_MARKER_WASH:
            return 13.26781416378316;
        case DP_BLEND_MODE_GREATER:
            return 1.2311923490965013;
        case DP_BLEND_MODE_GREATER_WASH:
            return 1.12436830449827;
        case DP_BLEND_MODE_PIGMENT:
        case DP_BLEND_MODE_OKLAB_RECOLOR:
            return 1.8239112725874662;
        case DP_BLEND_MODE_LIGHT_TO_ALPHA:
        case DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE:
            return 2.358524021530181;
        case DP_BLEND_MODE_DARK_TO_ALPHA:
        case DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE:
            return 3.217694133025759;
        case DP_BLEND_MODE_MULTIPLY_ALPHA:
            return 9.25372306805075;
        case DP_BLEND_MODE_DIVIDE_ALPHA:
            return 25.59207537101115;
        case DP_BLEND_MODE_BURN_ALPHA:
            return 25.75636865820838;
        case DP_BLEND_MODE_DODGE_ALPHA:
            return 24.927781399461743;
        case DP_BLEND_MODE_DARKEN_ALPHA:
            return 9.428535963091118;
        case DP_BLEND_MODE_LIGHTEN_ALPHA:
            return 9.195944990388313;
        case DP_BLEND_MODE_SUBTRACT_ALPHA:
            return 9.372368166089965;
        case DP_BLEND_MODE_ADD_ALPHA:
            return 9.593882529796232;
        case DP_BLEND_MODE_SCREEN_ALPHA:
            return 9.669376655132641;
        case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA:
            return 11.382953118031525;
        case DP_BLEND_MODE_OVERLAY_ALPHA:
            return 10.031855916955017;
        case DP_BLEND_MODE_HARD_LIGHT_ALPHA:
            return 9.624476732026144;
        case DP_BLEND_MODE_SOFT_LIGHT_ALPHA:
            return 10.277117639369473;
        case DP_BLEND_MODE_LINEAR_BURN_ALPHA:
            return 9.425370219146481;
        case DP_BLEND_MODE_LINEAR_LIGHT_ALPHA:
            return 9.64667819300269;
        case DP_BLEND_MODE_HUE_ALPHA:
            return 30.53966555171088;
        case DP_BLEND_MODE_SATURATION_ALPHA:
            return 21.320827520184544;
        case DP_BLEND_MODE_LUMINOSITY_ALPHA:
            return 15.480569034986544;
        case DP_BLEND_MODE_COLOR_ALPHA:
            return 36.0327723337178;
        case DP_BLEND_MODE_VIVID_LIGHT_ALPHA:
            return 25.7584740023068;
        case DP_BLEND_MODE_PIN_LIGHT_ALPHA:
            return 10.339963383314108;
        case DP_BLEND_MODE_DIFFERENCE_ALPHA:
            return 9.49685169550173;
        case DP_BLEND_MODE_DARKER_COLOR_ALPHA:
            return 12.489519031141867;
        case DP_BLEND_MODE_LIGHTER_COLOR_ALPHA:
            return 12.484537500961169;
        case DP_BLEND_MODE_SHADE_SAI_ALPHA:
            return 11.789792079969242;
        case DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA:
            return 12.05318495194156;
        case DP_BLEND_MODE_BURN_SAI_ALPHA:
            return 26.049373710111496;
        case DP_BLEND_MODE_DODGE_SAI_ALPHA:
            return 23.88518039215686;
        case DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA:
            return 26.047674509803922;
        case DP_BLEND_MODE_HARD_MIX_SAI_ALPHA:
            return 11.141510326797386;
        case DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA:
            return 11.495817078046905;
        case DP_BLEND_MODE_MARKER_ALPHA:
            return 13.306469842368319;
        case DP_BLEND_MODE_MARKER_ALPHA_WASH:
            return 13.50474983467897;
        case DP_BLEND_MODE_GREATER_ALPHA:
            return 1.1786877816224528;
        case DP_BLEND_MODE_GREATER_ALPHA_WASH:
            return 0.9867413071895424;
        case DP_BLEND_MODE_PIGMENT_ALPHA:
        case DP_BLEND_MODE_OKLAB_NORMAL:
            return 1.4677974471357171;
        case DP_BLEND_MODE_PIGMENT_AND_ERASER:
        case DP_BLEND_MODE_OKLAB_NORMAL_AND_ERASER:
            return 52.66137976931949;
        case DP_BLEND_MODE_REPLACE:
            return 1.0819777162629758;
        default:
            DP_debug("Unhandled blend mode %d", blend_mode);
            return 52.66137976931949;
        }
    }
}

double DP_dab_cost_pixel_square(bool indirect, int blend_mode)
{
    if (indirect) {
        return 16.626426451364857;
    }
    else {
        switch (blend_mode) {
        case DP_BLEND_MODE_ERASE:
        case DP_BLEND_MODE_ERASE_PRESERVE:
            return 4.732802191464821;
        case DP_BLEND_MODE_NORMAL:
            return 1.1348049134948097;
        case DP_BLEND_MODE_MULTIPLY:
            return 5.930073348712034;
        case DP_BLEND_MODE_DIVIDE:
            return 27.978639131103424;
        case DP_BLEND_MODE_BURN:
            return 28.481128565936174;
        case DP_BLEND_MODE_DODGE:
            return 27.67321946174548;
        case DP_BLEND_MODE_DARKEN:
            return 6.1625185620915035;
        case DP_BLEND_MODE_LIGHTEN:
            return 5.882162191464821;
        case DP_BLEND_MODE_SUBTRACT:
            return 6.260736701268742;
        case DP_BLEND_MODE_ADD:
            return 6.319855217224145;
        case DP_BLEND_MODE_RECOLOR:
            return 1.1853588542868128;
        case DP_BLEND_MODE_BEHIND:
        case DP_BLEND_MODE_BEHIND_PRESERVE:
            return 1.8964207766243752;
        case DP_BLEND_MODE_COLOR_ERASE:
        case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
            return 17.975427458669742;
        case DP_BLEND_MODE_SCREEN:
            return 6.946608981161091;
        case DP_BLEND_MODE_NORMAL_AND_ERASER:
            return 1.097094279123414;
        case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
            return 5.0306351403306415;
        case DP_BLEND_MODE_OVERLAY:
            return 7.475449557862361;
        case DP_BLEND_MODE_HARD_LIGHT:
            return 6.808575247981546;
        case DP_BLEND_MODE_SOFT_LIGHT:
            return 8.170940092272204;
        case DP_BLEND_MODE_LINEAR_BURN:
            return 6.3709530257593245;
        case DP_BLEND_MODE_LINEAR_LIGHT:
            return 6.773562891195694;
        case DP_BLEND_MODE_HUE:
            return 33.99501357170319;
        case DP_BLEND_MODE_SATURATION:
            return 23.45423850826605;
        case DP_BLEND_MODE_LUMINOSITY:
            return 15.691974256055362;
        case DP_BLEND_MODE_COLOR:
            return 41.53997974625145;
        case DP_BLEND_MODE_COMPARE_DENSITY_SOFT:
            return 0.8123571318723569;
        case DP_BLEND_MODE_COMPARE_DENSITY:
            return 0.8895821607074202;
        case DP_BLEND_MODE_VIVID_LIGHT:
            return 28.65552206074587;
        case DP_BLEND_MODE_PIN_LIGHT:
            return 7.579503844675124;
        case DP_BLEND_MODE_DIFFERENCE:
            return 6.574628842752787;
        case DP_BLEND_MODE_DARKER_COLOR:
            return 11.981171264898116;
        case DP_BLEND_MODE_LIGHTER_COLOR:
            return 11.979910788158401;
        case DP_BLEND_MODE_SHADE_SAI:
            return 6.292637377931564;
        case DP_BLEND_MODE_SHADE_SHINE_SAI:
            return 6.193483960015379;
        case DP_BLEND_MODE_BURN_SAI:
            return 28.037003444828912;
        case DP_BLEND_MODE_DODGE_SAI:
            return 24.79582888119954;
        case DP_BLEND_MODE_BURN_DODGE_SAI:
            return 27.93031070357555;
        case DP_BLEND_MODE_HARD_MIX_SAI:
            return 6.547208519800077;
        case DP_BLEND_MODE_DIFFERENCE_SAI:
            return 5.290716562860438;
        case DP_BLEND_MODE_MARKER:
            return 16.289019784698194;
        case DP_BLEND_MODE_MARKER_WASH:
            return 16.31307440984237;
        case DP_BLEND_MODE_GREATER:
            return 1.0618973010380623;
        case DP_BLEND_MODE_GREATER_WASH:
            return 0.956686674356017;
        case DP_BLEND_MODE_PIGMENT:
        case DP_BLEND_MODE_OKLAB_RECOLOR:
            return 1.8408407766243753;
        case DP_BLEND_MODE_LIGHT_TO_ALPHA:
        case DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE:
            return 1.9655729334871204;
        case DP_BLEND_MODE_DARK_TO_ALPHA:
        case DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE:
            return 3.0660452518262207;
        case DP_BLEND_MODE_MULTIPLY_ALPHA:
            return 11.36011664744329;
        case DP_BLEND_MODE_DIVIDE_ALPHA:
            return 32.007566935793925;
        case DP_BLEND_MODE_BURN_ALPHA:
            return 32.24220245290273;
        case DP_BLEND_MODE_DODGE_ALPHA:
            return 31.150895724721263;
        case DP_BLEND_MODE_DARKEN_ALPHA:
            return 11.58864183006536;
        case DP_BLEND_MODE_LIGHTEN_ALPHA:
            return 11.306184482891195;
        case DP_BLEND_MODE_SUBTRACT_ALPHA:
            return 11.51518336024606;
        case DP_BLEND_MODE_ADD_ALPHA:
            return 11.808597608612072;
        case DP_BLEND_MODE_SCREEN_ALPHA:
            return 11.89080334486736;
        case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA:
            return 14.04713661668589;
        case DP_BLEND_MODE_OVERLAY_ALPHA:
            return 12.363461730103806;
        case DP_BLEND_MODE_HARD_LIGHT_ALPHA:
            return 11.84152675124952;
        case DP_BLEND_MODE_SOFT_LIGHT_ALPHA:
            return 12.613499031141869;
        case DP_BLEND_MODE_LINEAR_BURN_ALPHA:
            return 11.575604159938486;
        case DP_BLEND_MODE_LINEAR_LIGHT_ALPHA:
            return 11.859590695886197;
        case DP_BLEND_MODE_HUE_ALPHA:
            return 38.22051636293733;
        case DP_BLEND_MODE_SATURATION_ALPHA:
            return 26.51944741253364;
        case DP_BLEND_MODE_LUMINOSITY_ALPHA:
            return 19.24481047289504;
        case DP_BLEND_MODE_COLOR_ALPHA:
            return 45.211455355632445;
        case DP_BLEND_MODE_VIVID_LIGHT_ALPHA:
            return 32.16740268358324;
        case DP_BLEND_MODE_PIN_LIGHT_ALPHA:
            return 12.773211141868513;
        case DP_BLEND_MODE_DIFFERENCE_ALPHA:
            return 11.694440338331411;
        case DP_BLEND_MODE_DARKER_COLOR_ALPHA:
            return 15.413685736255287;
        case DP_BLEND_MODE_LIGHTER_COLOR_ALPHA:
            return 15.402300076893502;
        case DP_BLEND_MODE_SHADE_SAI_ALPHA:
            return 14.561988281430219;
        case DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA:
            return 14.876952087658593;
        case DP_BLEND_MODE_BURN_SAI_ALPHA:
            return 32.54452279892349;
        case DP_BLEND_MODE_DODGE_SAI_ALPHA:
            return 29.805713963860054;
        case DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA:
            return 32.5070029834679;
        case DP_BLEND_MODE_HARD_MIX_SAI_ALPHA:
            return 13.75348336793541;
        case DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA:
            return 14.174775624759706;
        case DP_BLEND_MODE_MARKER_ALPHA:
            return 16.4616876816609;
        case DP_BLEND_MODE_MARKER_ALPHA_WASH:
            return 16.646093679354095;
        case DP_BLEND_MODE_GREATER_ALPHA:
            return 1.031316831987697;
        case DP_BLEND_MODE_GREATER_ALPHA_WASH:
            return 0.8380466820453671;
        case DP_BLEND_MODE_PIGMENT_ALPHA:
        case DP_BLEND_MODE_OKLAB_NORMAL:
            return 1.4359405690119185;
        case DP_BLEND_MODE_PIGMENT_AND_ERASER:
        case DP_BLEND_MODE_OKLAB_NORMAL_AND_ERASER:
            return 5.6664507497116485;
        case DP_BLEND_MODE_REPLACE:
            return 0.9043035601691657;
        default:
            DP_debug("Unhandled blend mode %d", blend_mode);
            return 45.211455355632445;
        }
    }
}

double DP_dab_cost_classic(bool indirect, int blend_mode)
{
    if (indirect) {
        return 0.0004715968997164729;
    }
    else {
        switch (blend_mode) {
        case DP_BLEND_MODE_ERASE:
        case DP_BLEND_MODE_ERASE_PRESERVE:
            return 9.915297002972446e-05;
        case DP_BLEND_MODE_NORMAL:
            return 4.4085872215069154e-05;
        case DP_BLEND_MODE_MULTIPLY:
            return 0.00011802693605621713;
        case DP_BLEND_MODE_DIVIDE:
            return 0.0004637788800666083;
        case DP_BLEND_MODE_BURN:
            return 0.00047166842409689325;
        case DP_BLEND_MODE_DODGE:
            return 0.00045893864765471934;
        case DP_BLEND_MODE_DARKEN:
            return 0.00012316968023617708;
        case DP_BLEND_MODE_LIGHTEN:
            return 0.00011838219384023194;
        case DP_BLEND_MODE_SUBTRACT:
            return 0.00012489064830405728;
        case DP_BLEND_MODE_ADD:
            return 0.00012612430442560124;
        case DP_BLEND_MODE_RECOLOR:
            return 4.512476922213722e-05;
        case DP_BLEND_MODE_BEHIND:
        case DP_BLEND_MODE_BEHIND_PRESERVE:
            return 5.6193642168509e-05;
        case DP_BLEND_MODE_COLOR_ERASE:
        case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
            return 0.00024788037802302924;
        case DP_BLEND_MODE_SCREEN:
            return 0.00013482138902747102;
        case DP_BLEND_MODE_NORMAL_AND_ERASER:
            return 4.5240435448734715e-05;
        case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
            return 0.00010256455646338412;
        case DP_BLEND_MODE_OVERLAY:
            return 0.00014382769799330358;
        case DP_BLEND_MODE_HARD_LIGHT:
            return 0.00013318125815146772;
        case DP_BLEND_MODE_SOFT_LIGHT:
            return 0.0001546731711521782;
        case DP_BLEND_MODE_LINEAR_BURN:
            return 0.00012614345451554442;
        case DP_BLEND_MODE_LINEAR_LIGHT:
            return 0.00013456722788073253;
        case DP_BLEND_MODE_HUE:
            return 0.0005562472477050041;
        case DP_BLEND_MODE_SATURATION:
            return 0.0003918350385991727;
        case DP_BLEND_MODE_LUMINOSITY:
            return 0.0002710046156649431;
        case DP_BLEND_MODE_COLOR:
            return 0.0006791853475157833;
        case DP_BLEND_MODE_COMPARE_DENSITY_SOFT:
            return 3.8926282107532524e-05;
        case DP_BLEND_MODE_COMPARE_DENSITY:
            return 4.1078227610413714e-05;
        case DP_BLEND_MODE_VIVID_LIGHT:
            return 0.000473162927533983;
        case DP_BLEND_MODE_PIN_LIGHT:
            return 0.00014501123299526982;
        case DP_BLEND_MODE_DIFFERENCE:
            return 0.00013017321748980077;
        case DP_BLEND_MODE_DARKER_COLOR:
            return 0.00021218148056390674;
        case DP_BLEND_MODE_LIGHTER_COLOR:
            return 0.00021353749909287868;
        case DP_BLEND_MODE_SHADE_SAI:
            return 0.00011749656542491326;
        case DP_BLEND_MODE_SHADE_SHINE_SAI:
            return 0.00011227989304760974;
        case DP_BLEND_MODE_BURN_SAI:
            return 0.0003754438132271272;
        case DP_BLEND_MODE_DODGE_SAI:
            return 0.0003382016100043489;
        case DP_BLEND_MODE_BURN_DODGE_SAI:
            return 0.0003695945587541001;
        case DP_BLEND_MODE_HARD_MIX_SAI:
            return 0.00035550991446711103;
        case DP_BLEND_MODE_DIFFERENCE_SAI:
            return 0.0001032210924410744;
        case DP_BLEND_MODE_MARKER:
            return 0.0002293077423458679;
        case DP_BLEND_MODE_MARKER_WASH:
            return 0.00022922795385521365;
        case DP_BLEND_MODE_GREATER:
            return 4.345813011763912e-05;
        case DP_BLEND_MODE_GREATER_WASH:
            return 4.1648832371949177e-05;
        case DP_BLEND_MODE_PIGMENT:
        case DP_BLEND_MODE_OKLAB_RECOLOR:
            return 0.002791007707983091;
        case DP_BLEND_MODE_LIGHT_TO_ALPHA:
        case DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE:
            return 7.350671444986241e-05;
        case DP_BLEND_MODE_DARK_TO_ALPHA:
        case DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE:
            return 7.43254706994095e-05;
        case DP_BLEND_MODE_MULTIPLY_ALPHA:
            return 0.00016752061552707985;
        case DP_BLEND_MODE_DIVIDE_ALPHA:
            return 0.0004182046297935377;
        case DP_BLEND_MODE_BURN_ALPHA:
            return 0.0004211714993858701;
        case DP_BLEND_MODE_DODGE_ALPHA:
            return 0.000407915362942623;
        case DP_BLEND_MODE_DARKEN_ALPHA:
            return 0.0001703472470594615;
        case DP_BLEND_MODE_LIGHTEN_ALPHA:
            return 0.00016677405399783318;
        case DP_BLEND_MODE_SUBTRACT_ALPHA:
            return 0.00016930474106728016;
        case DP_BLEND_MODE_ADD_ALPHA:
            return 0.0001729655204722038;
        case DP_BLEND_MODE_SCREEN_ALPHA:
            return 0.00017406824296309462;
        case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA:
            return 0.00020302451113837292;
        case DP_BLEND_MODE_OVERLAY_ALPHA:
            return 0.00017979317732889802;
        case DP_BLEND_MODE_HARD_LIGHT_ALPHA:
            return 0.0001733269917643949;
        case DP_BLEND_MODE_SOFT_LIGHT_ALPHA:
            return 0.00018328854693405684;
        case DP_BLEND_MODE_LINEAR_BURN_ALPHA:
            return 0.00017021847067055505;
        case DP_BLEND_MODE_LINEAR_LIGHT_ALPHA:
            return 0.0001744577142752399;
        case DP_BLEND_MODE_HUE_ALPHA:
            return 0.0004942035148266916;
        case DP_BLEND_MODE_SATURATION_ALPHA:
            return 0.0003518431159735317;
        case DP_BLEND_MODE_LUMINOSITY_ALPHA:
            return 0.00026321811048801984;
        case DP_BLEND_MODE_COLOR_ALPHA:
            return 0.0005823871803167536;
        case DP_BLEND_MODE_VIVID_LIGHT_ALPHA:
            return 0.0004207883845442791;
        case DP_BLEND_MODE_PIN_LIGHT_ALPHA:
            return 0.00018445087344395772;
        case DP_BLEND_MODE_DIFFERENCE_ALPHA:
            return 0.00017158486736010522;
        case DP_BLEND_MODE_DARKER_COLOR_ALPHA:
            return 0.0002177931110749165;
        case DP_BLEND_MODE_LIGHTER_COLOR_ALPHA:
            return 0.00021646057923896736;
        case DP_BLEND_MODE_SHADE_SAI_ALPHA:
            return 0.00021214086341557763;
        case DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA:
            return 0.000216243566190932;
        case DP_BLEND_MODE_BURN_SAI_ALPHA:
            return 0.00041595481105918076;
        case DP_BLEND_MODE_DODGE_SAI_ALPHA:
            return 0.000397497517382051;
        case DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA:
            return 0.00042367707304135906;
        case DP_BLEND_MODE_HARD_MIX_SAI_ALPHA:
            return 0.00040508415184097035;
        case DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA:
            return 0.00020841559784971777;
        case DP_BLEND_MODE_MARKER_ALPHA:
            return 0.0002299231927056776;
        case DP_BLEND_MODE_MARKER_ALPHA_WASH:
            return 0.00023343536947092788;
        case DP_BLEND_MODE_GREATER_ALPHA:
            return 4.259840350489732e-05;
        case DP_BLEND_MODE_GREATER_ALPHA_WASH:
            return 3.991356143038958e-05;
        case DP_BLEND_MODE_PIGMENT_ALPHA:
        case DP_BLEND_MODE_OKLAB_NORMAL:
            return 0.0028111838592867417;
        case DP_BLEND_MODE_PIGMENT_AND_ERASER:
        case DP_BLEND_MODE_OKLAB_NORMAL_AND_ERASER:
            return 0.000946904545003925;
        case DP_BLEND_MODE_REPLACE:
            return 4.137086309967501e-05;
        default:
            DP_debug("Unhandled blend mode %d", blend_mode);
            return 0.0028111838592867417;
        }
    }
}

double DP_dab_cost_mypaint(bool indirect, uint8_t lock_alpha, uint8_t colorize,
                           uint8_t posterize)
{
    if (indirect) {
        return 1.928548264957414e-05;
    }
    else {
        double cost = 0.0;
        if (lock_alpha != 0) {
            cost += 0.002730317666187609;
        }
        if (colorize != 0) {
            cost += 0.0006516152656088766;
        }
        if (posterize != 0) {
            cost += 0.00016140730604925454;
        }
        if (lock_alpha != UINT8_MAX && colorize != UINT8_MAX
            && posterize != UINT8_MAX) {
            cost += 0.002749581354641759;
        }
        return cost;
    }
}

double DP_dab_cost_mypaint_blend(bool indirect, int blend_mode)
{
    if (indirect) {
        return 4.2456682966997183e-08;
    }
    else {
        switch (blend_mode) {
        case DP_BLEND_MODE_ERASE:
        case DP_BLEND_MODE_ERASE_PRESERVE:
            return 7.541614698008654e-09;
        case DP_BLEND_MODE_NORMAL:
            return 3.939195609257487e-08;
        case DP_BLEND_MODE_MULTIPLY:
            return 4.089119835995608e-08;
        case DP_BLEND_MODE_DIVIDE:
            return 6.040754208270188e-08;
        case DP_BLEND_MODE_BURN:
            return 6.110570607380959e-08;
        case DP_BLEND_MODE_DODGE:
            return 6.005456005484819e-08;
        case DP_BLEND_MODE_DARKEN:
            return 4.0795967720515345e-08;
        case DP_BLEND_MODE_LIGHTEN:
            return 4.075626888426927e-08;
        case DP_BLEND_MODE_SUBTRACT:
            return 4.1077352140476554e-08;
        case DP_BLEND_MODE_ADD:
            return 4.1078399910348154e-08;
        case DP_BLEND_MODE_RECOLOR:
            return 3.768362552637266e-08;
        case DP_BLEND_MODE_BEHIND:
        case DP_BLEND_MODE_BEHIND_PRESERVE:
            return 3.738361408647195e-08;
        case DP_BLEND_MODE_COLOR_ERASE:
        case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
            return 3.9575548657853655e-08;
        case DP_BLEND_MODE_SCREEN:
            return 4.1722079868132806e-08;
        case DP_BLEND_MODE_NORMAL_AND_ERASER:
            return 4.021585246827428e-08;
        case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI:
            return 3.678405688216901e-08;
        case DP_BLEND_MODE_OVERLAY:
            return 4.2330135650283144e-08;
        case DP_BLEND_MODE_HARD_LIGHT:
            return 4.455175703469345e-08;
        case DP_BLEND_MODE_SOFT_LIGHT:
            return 4.6079289088607794e-08;
        case DP_BLEND_MODE_LINEAR_BURN:
            return 4.2559364414413736e-08;
        case DP_BLEND_MODE_LINEAR_LIGHT:
            return 4.3720177013269e-08;
        case DP_BLEND_MODE_HUE:
            return 6.773273409278836e-08;
        case DP_BLEND_MODE_SATURATION:
            return 5.76944002096378e-08;
        case DP_BLEND_MODE_LUMINOSITY:
            return 5.010610154290575e-08;
        case DP_BLEND_MODE_COLOR:
            return 7.529681763359905e-08;
        case DP_BLEND_MODE_COMPARE_DENSITY_SOFT:
            return 3.503952004595938e-08;
        case DP_BLEND_MODE_COMPARE_DENSITY:
            return 3.4819604791798554e-08;
        case DP_BLEND_MODE_VIVID_LIGHT:
            return 6.372419940180606e-08;
        case DP_BLEND_MODE_PIN_LIGHT:
            return 4.292771373371752e-08;
        case DP_BLEND_MODE_DIFFERENCE:
            return 4.162289098695492e-08;
        case DP_BLEND_MODE_DARKER_COLOR:
            return 4.5939237182437614e-08;
        case DP_BLEND_MODE_LIGHTER_COLOR:
            return 4.624518598494405e-08;
        case DP_BLEND_MODE_SHADE_SAI:
            return 3.640068952803898e-08;
        case DP_BLEND_MODE_SHADE_SHINE_SAI:
            return 3.827584834157443e-08;
        case DP_BLEND_MODE_BURN_SAI:
            return 4.203780785610748e-08;
        case DP_BLEND_MODE_DODGE_SAI:
            return 4.138260242973525e-08;
        case DP_BLEND_MODE_BURN_DODGE_SAI:
            return 4.428969814791948e-08;
        case DP_BLEND_MODE_HARD_MIX_SAI:
            return 3.6616995797086536e-08;
        case DP_BLEND_MODE_DIFFERENCE_SAI:
            return 3.709384750753796e-08;
        case DP_BLEND_MODE_MARKER:
            return 3.9932023251946e-08;
        case DP_BLEND_MODE_MARKER_WASH:
            return 4.0168120729679277e-08;
        case DP_BLEND_MODE_GREATER:
            return 3.438152056659623e-08;
        case DP_BLEND_MODE_GREATER_WASH:
            return 3.4585253597184605e-08;
        case DP_BLEND_MODE_PIGMENT:
        case DP_BLEND_MODE_OKLAB_RECOLOR:
            return 4.498751288240333e-08;
        case DP_BLEND_MODE_LIGHT_TO_ALPHA:
        case DP_BLEND_MODE_LIGHT_TO_ALPHA_PRESERVE:
            return 3.684075287410989e-08;
        case DP_BLEND_MODE_DARK_TO_ALPHA:
        case DP_BLEND_MODE_DARK_TO_ALPHA_PRESERVE:
            return 3.737942300698556e-08;
        case DP_BLEND_MODE_MULTIPLY_ALPHA:
            return 3.885968899780341e-08;
        case DP_BLEND_MODE_DIVIDE_ALPHA:
            return 4.397664779406111e-08;
        case DP_BLEND_MODE_BURN_ALPHA:
            return 4.416838968056343e-08;
        case DP_BLEND_MODE_DODGE_ALPHA:
            return 4.367256169354863e-08;
        case DP_BLEND_MODE_DARKEN_ALPHA:
            return 3.895049572000851e-08;
        case DP_BLEND_MODE_LIGHTEN_ALPHA:
            return 3.8894265403566114e-08;
        case DP_BLEND_MODE_SUBTRACT_ALPHA:
            return 3.999896410485361e-08;
        case DP_BLEND_MODE_ADD_ALPHA:
            return 3.9172855770536395e-08;
        case DP_BLEND_MODE_SCREEN_ALPHA:
            return 3.919776940970549e-08;
        case DP_BLEND_MODE_LUMINOSITY_SHINE_SAI_ALPHA:
            return 3.856363579963984e-08;
        case DP_BLEND_MODE_OVERLAY_ALPHA:
            return 3.950476598208351e-08;
        case DP_BLEND_MODE_HARD_LIGHT_ALPHA:
            return 4.127596274058157e-08;
        case DP_BLEND_MODE_SOFT_LIGHT_ALPHA:
            return 4.1791814774031335e-08;
        case DP_BLEND_MODE_LINEAR_BURN_ALPHA:
            return 3.909287600367113e-08;
        case DP_BLEND_MODE_LINEAR_LIGHT_ALPHA:
            return 4.0192103017851395e-08;
        case DP_BLEND_MODE_HUE_ALPHA:
            return 4.814234796578303e-08;
        case DP_BLEND_MODE_SATURATION_ALPHA:
            return 4.362448069832978e-08;
        case DP_BLEND_MODE_LUMINOSITY_ALPHA:
            return 4.1749903979167445e-08;
        case DP_BLEND_MODE_COLOR_ALPHA:
            return 4.729668126052932e-08;
        case DP_BLEND_MODE_VIVID_LIGHT_ALPHA:
            return 4.606031281204443e-08;
        case DP_BLEND_MODE_PIN_LIGHT_ALPHA:
            return 3.933223320989382e-08;
        case DP_BLEND_MODE_DIFFERENCE_ALPHA:
            return 3.910614775537803e-08;
        case DP_BLEND_MODE_DARKER_COLOR_ALPHA:
            return 4.0332853437269315e-08;
        case DP_BLEND_MODE_LIGHTER_COLOR_ALPHA:
            return 4.0110027711242943e-08;
        case DP_BLEND_MODE_SHADE_SAI_ALPHA:
            return 3.92599370887536e-08;
        case DP_BLEND_MODE_SHADE_SHINE_SAI_ALPHA:
            return 4.324425665381455e-08;
        case DP_BLEND_MODE_BURN_SAI_ALPHA:
            return 4.433009549741329e-08;
        case DP_BLEND_MODE_DODGE_SAI_ALPHA:
            return 4.34149267240103e-08;
        case DP_BLEND_MODE_BURN_DODGE_SAI_ALPHA:
            return 4.781800498108633e-08;
        case DP_BLEND_MODE_HARD_MIX_SAI_ALPHA:
            return 3.834011156036573e-08;
        case DP_BLEND_MODE_DIFFERENCE_SAI_ALPHA:
            return 4.021887935901444e-08;
        case DP_BLEND_MODE_MARKER_ALPHA:
            return 4.0642411224889024e-08;
        case DP_BLEND_MODE_MARKER_ALPHA_WASH:
            return 4.048373229877933e-08;
        case DP_BLEND_MODE_GREATER_ALPHA:
            return 3.484777815945706e-08;
        case DP_BLEND_MODE_GREATER_ALPHA_WASH:
            return 3.47694282568365e-08;
        case DP_BLEND_MODE_PIGMENT_ALPHA:
        case DP_BLEND_MODE_OKLAB_NORMAL:
            return 4.558392677709149e-08;
        case DP_BLEND_MODE_PIGMENT_AND_ERASER:
        case DP_BLEND_MODE_OKLAB_NORMAL_AND_ERASER:
            return 2.075977879692025e-07;
        case DP_BLEND_MODE_REPLACE:
            return 3.678813154278078e-08;
        default:
            DP_debug("Unhandled blend mode %d", blend_mode);
            return 2.075977879692025e-07;
        }
    }
}
