#!/usr/bin/env groovy

// Default CMake flags for release builds.
defaultReleaseBuildFlags = [
    'CAF_ENABLE_RUNTIME_CHECKS:BOOL=yes',
    'CAF_NO_OPENCL:BOOL=yes',
    'CAF_INSTALL_UNIT_TESTS:BOOL=yes',
]

// Default CMake flags for debug builds.
defaultDebugBuildFlags = defaultReleaseBuildFlags + [
    'CAF_ENABLE_ADDRESS_SANITIZER:BOOL=yes',
    'CAF_LOG_LEVEL:STRING=TRACE',
]

defaultBuildFlags = [
  debug: defaultDebugBuildFlags,
  release: defaultReleaseBuildFlags,
]

// CMake flags by OS and build type.
buildFlags = [
    macOS: [
        debug: defaultDebugBuildFlags + [
            'OPENSSL_ROOT_DIR=/usr/local/opt/openssl',
            'OPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include',
        ],
        release: defaultReleaseBuildFlags + [
            'OPENSSL_ROOT_DIR=/usr/local/opt/openssl',
            'OPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include',
        ],
    ],
    Windows: [
        debug: defaultDebugBuildFlags + [
            'CAF_BUILD_STATIC_ONLY:BOOL=yes',
        ],
        release: defaultReleaseBuildFlags + [
            'CAF_BUILD_STATIC_ONLY:BOOL=yes',
        ],
    ],
]

// Our build matrix. The keys are the operating system labels and the values
// are lists of tool labels.
buildMatrix = [
    ['Linux', [
        builds: ['debug'],
        tools: ['gcc4.8', 'gcc4.9', 'gcc5', 'gcc6', 'gcc7'],
    ]],
    ['Linux', [
        builds: ['debug'],
        tools: ['gcc8'],
        extraSteps: ['coverageReport'],
    ]],
    ['Linux', [
        builds: ['release'],
        tools: ['gcc8', 'clang'],
    ]],
    ['macOS', [
        builds: ['debug', 'release'],
        tools: ['clang'],
    ]],
    ['FreeBSD', [
        builds: ['debug'], // no release build for now, because it takes 1h
        tools: ['clang'],
    ]],
    ['Windows', [
        // TODO: debug build currently broken
        //builds: ['debug', 'release'],
        builds: ['release'],
        tools: ['msvc'],
    ]],
]

// Optional environment variables for combinations of labels.
buildEnvironments = [
    'macOS && gcc': ['CXX=g++'],
    'Linux && clang': ['CXX=clang++'],
]

// Adds additional context information to commits on GitHub.
def setBuildStatus(context, state, message) {
    echo "set ${context} result for commit ${env.GIT_COMMIT} to $state: $message"
    step([
        $class: 'GitHubCommitStatusSetter',
        commitShaSource: [
            $class: 'ManuallyEnteredShaSource',
            sha: env.GIT_COMMIT,
        ],
        reposSource: [
            $class: 'ManuallyEnteredRepositorySource',
            url: env.GIT_URL,
        ],
        contextSource: [
            $class: 'ManuallyEnteredCommitContextSource',
            context: context,
        ],
        errorHandlers: [[
            $class: 'ChangingBuildStatusErrorHandler',
            result: 'SUCCESS',
        ]],
        statusResultSource: [
            $class: 'ConditionalStatusResultSource',
            results: [[
                $class: 'AnyBuildResult',
                state: state,
                message: message,
            ]]
        ],
    ]);
}

// Creates coverage reports via the Cobertura plugin.
def coverageReport(buildId) {
    echo "Create coverage report for build ID $buildId"
    // Paths we wish to ignore in the coverage report.
    def installDir = "$WORKSPACE/$buildId"
    def excludePaths = [
        "/usr/",
        "$WORKSPACE/caf-sources/examples",
        "$WORKSPACE/caf-sources/tools",
        "$WORKSPACE/caf-sources/libcaf_test",
        "$WORKSPACE/caf-sources/libcaf_core/test",
        "$WORKSPACE/caf-sources/libcaf_io/test",
        "$WORKSPACE/caf-sources/libcaf_openssl/test",
        "$WORKSPACE/caf-sources/libcaf_opencl",
        "$WORKSPACE/caf-sources/libcaf_core/caf/scheduler/profiled_coordinator.hpp",
    ]
    def excludePathsStr = excludePaths.join(',')
    dir('caf-sources') {
        try {
            withEnv(['ASAN_OPTIONS=verify_asan_link_order=false:detect_leaks=0']) {
                sh """
                    kcov --exclude-path=$excludePathsStr kcov-result ./build/caf-test &> kcov_output.txt
                    find . -name 'coverage.json' -exec mv {} result.json \\;
                """
            }
            stash includes: 'result.json', name: 'coverage-result'
            archiveArtifacts '**/cobertura.xml'
            cobertura([
                autoUpdateHealth: false,
                autoUpdateStability: false,
                coberturaReportFile: '**/cobertura.xml',
                conditionalCoverageTargets: '70, 0, 0',
                failUnhealthy: false,
                failUnstable: false,
                lineCoverageTargets: '80, 0, 0',
                maxNumberOfBuilds: 0,
                methodCoverageTargets: '80, 0, 0',
                onlyStable: false,
                sourceEncoding: 'ASCII',
                zoomCoverageChart: false,
            ])
        } catch (Exception e) {
            echo "exception: $e"
            archiveArtifacts 'kcov_output.txt'
        }
    }
}

// Wraps `cmakeBuild`.
def cmakeSteps(buildType, cmakeArgs, buildId) {
    def installDir = "$WORKSPACE/$buildId"
    dir('caf-sources') {
        // Configure and build.
        cmakeBuild([
            buildDir: 'build',
            buildType: buildType,
            cmakeArgs: (cmakeArgs + [
              "CMAKE_INSTALL_PREFIX=\"$installDir\"",
            ]).collect { x -> '-D' + x }.join(' '),
            installation: 'cmake in search path',
            sourceDir: '.',
            steps: [[
                args: '--target install',
                withCmake: true,
            ]],
        ])
        // Run unit tests.
        try {
            ctest([
                arguments: '--output-on-failure',
                installation: 'cmake in search path',
                workingDir: 'build',
            ])
            writeFile file: "${buildId}.success", text: "success\n"
        } catch (Exception) {
            writeFile file: "${buildId}.failure", text: "failure\n"
        }
        stash includes: "${buildId}.*", name: buildId
    }
    // Only generate artifacts for the master branch.
    if (PrettyJobBaseName == 'master') {
      zip([
          archive: true,
          dir: buildId,
          zipFile: "${buildId}.zip",
      ])
    }
}

// Builds `name` with CMake and runs the unit tests.
def buildSteps(buildType, cmakeArgs, buildId) {
    echo "build stage: $STAGE_NAME"
    deleteDir()
    dir(buildId) {
        // Create directory.
    }
    unstash('caf-sources')
    if (STAGE_NAME.contains('Windows')) {
        echo "Windows build on $NODE_NAME"
        withEnv(['PATH=C:\\Windows\\System32;C:\\Program Files\\CMake\\bin;C:\\Program Files\\Git\\cmd;C:\\Program Files\\Git\\bin']) {
            cmakeSteps(buildType, cmakeArgs, buildId)
        }
    } else {
        echo "Unix build on $NODE_NAME"
        withEnv(["label_exp=" + STAGE_NAME.toLowerCase(), "ASAN_OPTIONS=detect_leaks=0"]) {
            cmakeSteps(buildType, cmakeArgs, buildId)
        }
    }
}

// Builds a stage for given builds. Results in a parallel stage `if builds.size() > 1`.
def makeBuildStages(matrixIndex, os, builds, lblExpr, settings) {
    builds.collectEntries { buildType ->
        def id = "$matrixIndex $lblExpr: $buildType"
        [
            (id):
            {
                node(lblExpr) {
                    stage(id) {
                      try {
                          def buildId = "${lblExpr}_${buildType}".replace(' && ', '_')
                          withEnv(buildEnvironments[lblExpr] ?: []) {
                              buildSteps(buildType, (buildFlags[os] ?: defaultBuildFlags)[buildType], buildId)
                              (settings['extraSteps'] ?: []).each { fun -> "$fun"(buildId) }
                          }
                      } finally {
                          cleanWs()
                      }
                    }
                }
            }
        ]
    }
}

pipeline {
    options {
        buildDiscarder(logRotator(numToKeepStr: '20', artifactNumToKeepStr: '5'))
    }
    agent none
    environment {
        LD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
        DYLD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
        PrettyJobBaseName = env.JOB_BASE_NAME.replace('%2F', '/')
        PrettyJobName = "CAF build #${env.BUILD_NUMBER} for $PrettyJobBaseName"
    }
    stages {
        stage('Git Checkout') {
            agent { label 'master' }
            steps {
                deleteDir()
                dir('caf-sources') {
                    checkout scm
                    sh './scripts/get-release-version.sh'
                    /// We really don't need to copy the .git folder around.
                    dir('.git') {
                        deleteDir()
                    }
                }
                zip([
                    archive: true,
                    dir: 'caf-sources',
                    zipFile: 'sources.zip',
                ])
                stash includes: 'caf-sources/**', name: 'caf-sources'
            }
        }
        stage('Builds') {
            steps {
                script {
                    // Create stages for building everything in our build matrix in
                    // parallel.
                    def xs = [:]
                    buildMatrix.eachWithIndex { entry, index ->
                        def (os, settings) = entry
                        settings['tools'].eachWithIndex { tool, toolIndex ->
                            def matrixIndex = "[$index:$toolIndex]"
                            def builds = settings['builds']
                            def labelExpr = "$os && $tool"
                            xs << makeBuildStages(matrixIndex, os, builds, labelExpr, settings)
                        }
                    }
                    parallel xs
                }
            }
        }
        stage('Check Test Results') {
            agent { label 'master' }
            steps {
                script {
                    dir('tmp') {
                        // Compute the list of all build IDs.
                        def buildIds = []
                        buildMatrix.each { entry ->
                            def (os, settings) = entry
                            settings['tools'].each { tool ->
                                settings['builds'].each {
                                    buildIds << "${os}_${tool}_${it}"
                                }
                            }
                        }
                        // Compute how many tests have succeeded
                        def builds = buildIds.size()
                        def successes = buildIds.inject(0) { result, buildId ->
                            try { unstash buildId }
                            catch (Exception) { }
                            result + (fileExists("${buildId}.success") ? 1 : 0)
                        }
                        echo "$successes unit tests tests of $builds were successful"
                        if (builds == successes) {
                            setBuildStatus('unit-tests', 'SUCCESS', 'All builds passed the unit tests')
                        } else {
                            def failures = builds - successes
                            setBuildStatus('unit-tests', 'FAILURE', "$failures/$builds builds failed to run the unit tests")
                        }
                        // Get the coverage result.
                        try {
                            unstash 'coverage-result'
                            if (fileExists('result.json')) {
                                def resultJson = readJSON file: 'result.json'
                                setBuildStatus('coverage', 'SUCCESS', resultJson['percent_covered'] + '% coverage')
                            } else {
                              setBuildStatus('coverage', 'FAILURE', 'Unable to get coverage report')
                            }
                        } catch (Exception) {
                            setBuildStatus('coverage', 'FAILURE', 'Unable to generate coverage report')
                        }
                    }
                }
            }
        }
        stage('Documentation') {
            agent { label 'pandoc' }
            steps {
                deleteDir()
                unstash('caf-sources')
                dir('caf-sources') {
                    // Configure and build.
                    cmakeBuild([
                        buildDir: 'build',
                        installation: 'cmake in search path',
                        sourceDir: '.',
                        cmakeArgs: '-DCAF_BUILD_TEX_MANUAL=yes',
                        steps: [[
                            args: '--target doc',
                            withCmake: true,
                        ]],
                    ])
                    sshagent(['84d71a75-cbb6-489a-8f4c-d0e2793201e9']) {
                        sh """
                            if [ "${env.GIT_BRANCH}" = "master" ]; then
                                rsync -e "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null" -r -z --delete build/doc/html/ www.inet.haw-hamburg.de:/users/www/www.actor-framework.org/html/doc
                                scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null build/doc/manual.pdf www.inet.haw-hamburg.de:/users/www/www.actor-framework.org/html/pdf/manual.pdf
                            fi
                        """
                    }
                }
                dir('read-the-docs') {
                    git([
                        credentialsId: '9b054212-9bb4-41fd-ad8e-b7d47495303f',
                        url: 'git@github.com:actor-framework/read-the-docs.git',
                    ])
                    sh """
                        if [ "${env.GIT_BRANCH}" = "master" ]; then
                            cp ../caf-sources/build/doc/rst/* .
                            if [ -n "\$(git status --porcelain)" ]; then
                                git add .
                                git commit -m "Update Manual"
                                git push --set-upstream origin master
                                if [ -z "\$(grep 'exp.sha' ../caf-sources/release.txt)" ] ; then
                                    git tag \$(cat ../caf-sources/release.txt)
                                    git push origin \$(cat ../caf-sources/release.txt)
                                fi
                            fi
                        fi
                    """
                }
            }
        }
    }
    post {
        success {
            emailext(
                subject: "✅ $PrettyJobName succeeded",
                recipientProviders: [culprits(), developers(), requestor(), upstreamDevelopers()],
                body: "Check console output at ${env.BUILD_URL}.",
            )
        }
        failure {
            emailext(
                subject: "⛔️ $PrettyJobName failed",
                attachLog: true,
                compressLog: true,
                recipientProviders: [culprits(), developers(), requestor(), upstreamDevelopers()],
                body: "Check console output at ${env.BUILD_URL} or see attached log.\n",
            )
        }
    }
}
