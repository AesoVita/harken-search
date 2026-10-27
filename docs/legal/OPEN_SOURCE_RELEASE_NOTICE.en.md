Harken Search Open-Source Release Notice

Release Date: October 28, 2026

I. Release Statement

This Notice describes the scope of Harken Search’s open-source release, the official release entity, applicable licenses, third-party components, the relationship between source code and binary releases, official download hosting, trademarks, warranty disclaimers, and contact channels. This Notice does not replace the LICENSE, NOTICE, LICENSES/, third-party license texts, User Agreement, Privacy Policy, Security Policy, or any other applicable terms maintained in the repository. If this Notice conflicts with an applicable open-source license, the applicable open-source license will prevail.

XD Network Inc. (“XD”) is the official release entity for Harken Search and is responsible for the public release of the Harken Search project and the maintenance of its official release channels. Contributors, project maintainers, Pull Request submitters, Issue or Discussion participants, Fork maintainers, and other community participants do not become agents of XD or the official release entity merely by participating in the project.

II. Scope of the Open-Source Release

2.1 Source Code

XD publicly releases all source code in the official Harken Search repository, including implemented feature code and any build, test, release, and legal materials that may lawfully be made public.

The publicly released source code may include implemented feature code, interface code, or bridge code that is not included in a particular official release of the EXE, installer, or update package. The publication of such source code does not mean that the corresponding functionality is included in the applicable official program, or that XD provides the functionality, related services, technical support, or authorization for third-party software in that version.

For purposes of this Notice, “public release” means making the source code and corresponding runnable programs described in this Notice publicly available to unspecified users.

XD may change the names, logos, domain names, or other identifiers of the Products in accordance with its branding, product development, or operational arrangements. Unless otherwise expressly stated by XD, any such change will not affect the validity or continued application of this Notice, or any rights or obligations that have arisen or are to be performed under this Notice. A Product under a changed name will continue to be deemed the corresponding Product referred to in this Notice.

2.2 Corresponding Runnable Programs

XD publicly releases the official Harken Search main application EXE, installers, update packages, and other corresponding binary programs built from the relevant Harken Search source code. The specific source code, components and binary release scope are subject to the relevant repositories, licenses and version release materials.

2.3 Source-Code Scope and Official-Program Boundaries

The official Harken Search repository is released in its entirety. The repository may contain implemented feature code, interface code, or bridge code that is not included in a particular official Harken Search EXE, installer, or update package.

An official program provides only the functionality expressly identified in its official release notes and accompanying release materials. The presence of functionality or bridge code in the public source repository does not mean that such functionality is provided to users in a particular release or supported by XD. Compiling, modifying, or enabling other publicly available source code independently does not constitute use of an official function or official service provided by XD.

2.4 Matters Outside the Open-Source Release

The following are outside the scope of Harken Search’s open-source release:

AK/SK credentials, access tokens, account passwords, signing private keys, code-signing certificates, upload credentials, and other credentials used by OSS, CDN, websites, CI/CD systems, or other operational systems;

Internal configurations, documents, data, test materials, personal information, trade secrets, and other materials that may not be made public under applicable law or security requirements;

Third-party code, documentation, materials, models, fonts, icons, audio, or other content for which there is no lawful basis for public disclosure, copying, modification, or redistribution; and

Proprietary SDKs, standalone bridge artifacts, model-service configurations, or other restricted materials that are not included in the relevant official release.

2.5 Subsequent Versions and Additional Content

Harken Search may add, modify, suspend, or remove code, features, components, documentation, release materials, or related services based on product development, community contributions, changes to third-party services, or other circumstances. The applicable licenses, third-party rights, functionality, and release conditions for any new or modified content are governed by the LICENSE, NOTICE, applicable third-party license texts, release notes, and other materials accompanying the relevant version. Unless otherwise required by applicable law or an applicable open-source license, the release materials for one version do not automatically apply to another version. Nothing in this Section affects rights that users have lawfully obtained under an applicable open-source license.

III. Release Entity and Download Support

XD is responsible for determining Harken Search’s official releases, public repository, and official release materials, and may provide release notes and download information in accordance with its release arrangements.

XD or service providers designated by XD may use official websites, object-storage services (OSS), CDNs, or other official download channels to upload, host, and make available for manual download or automatic update official binary programs that have been publicly released. Such download support does not change XD’s status as the official release entity for the source code and corresponding runnable programs, and does not change the rights granted under the applicable open-source licenses.

AK/SK credentials, upload credentials, signing private keys, and other controlled credentials used for OSS, GitHub, CDN, or other release channels are not open-source materials and should not be included in the public repository, client, installer, or update package. The specific GitHub repository or Release, OSS/CDN address, fallback conditions, update frequency, user controls, signature-verification method, and rollback policy for official downloads and automatic updates are governed by the official release page, client documentation, and Privacy Policy.

IV. Open-Source and Third-Party Licenses

4.1 Harken Search Project License

The Harken Search source code and corresponding object code are released under the Apache License 2.0 stated in the repository’s root LICENSE file. The specific scope and conditions of the license are governed by that license text.

To the extent the Apache License 2.0 applies, you may use, copy, modify, and distribute the relevant source code and object code in accordance with that license. This Notice, the User Agreement, website terms, or other documents do not limit rights granted under the applicable open-source license.

4.2 Third-Party Components

Harken Search includes or may include third-party open-source components, system components, resources, or other third-party materials. Such components remain subject to their respective licenses, copyright notices, redistribution conditions, and other applicable terms.

The versions, licenses, sources, uses, and legal materials for third-party components are identified in the official repository’s NOTICES.md, LICENSES/ directory, applicable component license texts, the Legal/ directory accompanying a release, and the SBOM for the relevant version. If you copy, modify, or redistribute source code or binary programs containing third-party components, you must comply with the applicable license requirements.

Binary SBOMs and accompanying legal materials are based on the components and dependencies actually packaged, linked, or distributed with the relevant build. Code, feature modules, bridge implementations, or related dependencies present in the public source repository but not included in a particular release artifact do not thereby constitute components distributed with that release’s binaries or functionality provided to users.

4.3 Release License Review

XD may, based on the relevant release, conduct appropriate correspondence checks among NOTICES.md, LICENSES/, the SBOM, the actual build artifacts, and the Legal/ materials accompanying the release. Third-party code, proprietary components, or other protected materials without a confirmed lawful basis for redistribution will not be included as part of the relevant official release materials.

Where a component is subject to specific linking, redistribution, or other license conditions, the relevant release materials are governed by the applicable license and the legal materials accompanying the release. Specific component and license information is governed by the official repository and the release materials for the relevant version.

V. Correspondence and Verification of Source Code and Runnable Programs

The official release page or related release materials may provide the following information to help users understand the origin and status of release materials:

the official repository address and the applicable tag, commit, or other version identifier;

the source-code version and the version of the official runnable program;

publicly available build instructions, build scripts, or build-environment requirements;

the file names, release dates, and hash values of installers, EXEs, update packages, or other binary programs;

where applicable, Windows Authenticode signature information, signing-certificate information, or other system verification information;

the SBOM, LICENSE, NOTICES.md, LICENSES/, third-party license texts, and other Legal/ materials for the relevant version; and

release notes, known issues, security fixes, upgrade, rollback, or withdrawal information.

Users may use the repository version identifier, official release notes, official download channels, file hashes, client-side automatic-update verification, and applicable operating-system signature information to make a reasonable assessment of the correspondence between release materials and the integrity of files. Formal signing and uploading may be performed through controlled release systems, and XD does not undertake that users will be able to reproduce the complete signing or upload process solely from publicly available materials.

VI. Trademarks, Branding, and Official Status

The Apache License 2.0 and other open-source licenses grant rights in the applicable source code and object code. They do not, by themselves, grant rights to use “Harken Search,” “Harken,” “XD,” related logos, icons, domain names, product appearance, brand identifiers, or official endorsements.

Without permission from XD or the relevant rights holder, you may not use the foregoing trademarks or brand identifiers in a manner likely to cause confusion, or imply that your Fork, modified version, redistributed version, or service has been reviewed, released, certified, warranted, or supported by XD or the official project.

If you modify the source code and distribute your own version, you should clearly and reasonably identify the modifications and the responsible party so that users are not misled into believing that it is an official version.

VII. Warranty Disclaimer and Limitation of Responsibility

To the extent permitted by applicable law and the relevant open-source licenses, the Harken Search source code, runnable programs, documentation, examples, release materials, and related services are provided on an “as is” and “as available” basis. XD does not warrant that they are error-free, uninterrupted, completely secure, fully compatible, fit for any particular purpose, suitable for any particular requirement, or free from infringement of third-party rights.

Harken Search primarily processes audio files, indexes, search information, metadata, transcription text, and other local content on the user’s device. You are responsible for ensuring that you have obtained the rights necessary to process, import, scan, transcribe, edit, export, or commercially use such content, and should back up important audio, indexes, configurations, databases, and other data before use. The metadata-writing function may modify original audio files.

If XD separately provides technical support, hosted downloads, compatibility certification, enterprise services, training, commercial warranties, or other independent services, their scope, fees, and conditions are governed by the applicable rules or written agreement. Such services do not automatically form part of the open-source license.

VIII. Security, Community, and Contributions

If you discover a security vulnerability, exposed secret, exploitable defect, or other risk involving non-public information, we recommend that you submit it through the channel specified in the repository’s SECURITY.md rather than through a public Issue, Pull Request, Discussion, or other public channel.

If the project accepts external contributions, contributors should comply with the then-current CONTRIBUTING, SECURITY.md, code of conduct, DCO/CLA, open-source licenses, and other project rules. Whether contribution content is merged, released, or retained is subject to project maintenance and release arrangements; contributors do not become agents of XD or official release entities merely by submitting or having content merged.

IX. Contact Information

For questions regarding Harken Search’s open-source licenses, third-party components, official release materials, trademark use, download channels, security matters, or infringement complaints, please contact:

Security reports: .github/SECURITY.md in the repository

Other contact channel: harken-support@xd.com

X. Miscellaneous

This Notice is intended solely to describe Harken Search’s public release arrangements and does not constitute any additional warranty, agency, partnership, employment, or other legal relationship with any user.

Updates to this Notice will be published through the official repository, release page, website, or other reasonable channel.
